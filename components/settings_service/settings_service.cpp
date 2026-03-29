#include "settings_service.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sys/time.h>
#include "driver/i2c_master.h"
#include "esp_app_desc.h"
#include "esp_check.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif_sntp.h"
#include "esp_psram.h"
#include "nvs_flash.h"
#include "bsp/display.h"
#include "bsp/esp32_s3_touch_amoled_2_06.h"
#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"

namespace storybook {

namespace {

constexpr const char *TAG = "SettingsService";
constexpr const char *NVS_NAMESPACE = "settings";
constexpr const char *KEY_BRIGHTNESS = "brightness";
constexpr const char *KEY_VOLUME = "volume";
constexpr const char *KEY_AUTO_SYNC = "auto_sync";
constexpr const char *KEY_WIFI_SSID = "wifi_ssid";
constexpr const char *KEY_WIFI_PASS = "wifi_pass";
constexpr int DEFAULT_BRIGHTNESS = 80;
constexpr int DEFAULT_VOLUME = 60;
constexpr int WIFI_RETRY_LIMIT = 5;
constexpr int WIFI_SCAN_RESULT_LIMIT = 15;
constexpr time_t VALID_TIME_EPOCH = 1704067200;
constexpr const char *DEFAULT_TIME_ZONE = "CST-8";
constexpr uint8_t PMU_I2C_ADDRESS = 0x34;
constexpr int PMU_I2C_TIMEOUT_MS = 1000;

XPowersPMU g_pmu;
i2c_master_dev_handle_t g_pmu_dev_handle = nullptr;
bool g_pmu_ready = false;

constexpr esp_codec_dev_sample_info_t SPEAKER_SAMPLE_INFO = {
    .bits_per_sample = 16,
    .channel = 2,
    .channel_mask = 0x03,
    .sample_rate = 22050,
    .mclk_multiple = 0,
};

int clamp_percent(int value)
{
    return std::clamp(value, 0, 100);
}

std::string format_storage_size(size_t bytes)
{
    if (bytes == 0) {
        return "Unavailable";
    }

    char buffer[24] = {0};
    std::snprintf(buffer, sizeof(buffer), "%u MB", static_cast<unsigned>(bytes / (1024 * 1024)));
    return buffer;
}

std::string format_mac_address(const uint8_t *mac)
{
    char buffer[24] = {0};
    std::snprintf(buffer, sizeof(buffer), "%02X:%02X:%02X:%02X:%02X:%02X",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return buffer;
}

std::string build_chip_summary()
{
    esp_chip_info_t chip_info = {};
    esp_chip_info(&chip_info);

    char buffer[48] = {0};
    std::snprintf(buffer, sizeof(buffer), "ESP32-S3 rev %d, %d cores", chip_info.revision, chip_info.cores);
    return buffer;
}

std::string format_temperature(float temperature_c)
{
    char buffer[24] = {0};
    std::snprintf(buffer, sizeof(buffer), "%.1f C", static_cast<double>(temperature_c));
    return buffer;
}

std::string format_voltage(uint16_t millivolt)
{
    char buffer[24] = {0};
    std::snprintf(buffer, sizeof(buffer), "%.2f V", static_cast<double>(millivolt) / 1000.0);
    return buffer;
}

int pmu_register_read(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint8_t len)
{
    (void)devAddr;
    if (g_pmu_dev_handle == nullptr) {
        return -1;
    }

    return (i2c_master_transmit_receive(g_pmu_dev_handle, &regAddr, 1, data, len, PMU_I2C_TIMEOUT_MS) == ESP_OK) ? 0 : -1;
}

int pmu_register_write_byte(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint8_t len)
{
    (void)devAddr;
    if (g_pmu_dev_handle == nullptr) {
        return -1;
    }

    std::vector<uint8_t> buffer(static_cast<size_t>(len) + 1, 0);
    buffer[0] = regAddr;
    if ((len > 0) && (data != nullptr)) {
        std::memcpy(buffer.data() + 1, data, len);
    }

    return (i2c_master_transmit(g_pmu_dev_handle, buffer.data(), buffer.size(), PMU_I2C_TIMEOUT_MS) == ESP_OK) ? 0 : -1;
}

esp_err_t ensure_pmu_ready()
{
    if (g_pmu_ready) {
        return ESP_OK;
    }

    i2c_master_bus_handle_t bus_handle = bsp_i2c_get_handle();
    ESP_RETURN_ON_FALSE(bus_handle != nullptr, ESP_FAIL, TAG, "Get I2C bus failed");

    if (g_pmu_dev_handle == nullptr) {
        i2c_device_config_t dev_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = PMU_I2C_ADDRESS,
            .scl_speed_hz = CONFIG_BSP_I2C_CLK_SPEED_HZ,
            .scl_wait_us = 0,
            .flags = {
                .disable_ack_check = 0,
            },
        };
        ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus_handle, &dev_config, &g_pmu_dev_handle), TAG,
                            "Add PMU I2C device failed");
    }

    ESP_RETURN_ON_FALSE(g_pmu.begin(PMU_I2C_ADDRESS, pmu_register_read, pmu_register_write_byte), ESP_FAIL, TAG,
                        "Init AXP2101 failed");

    g_pmu.enableVbusVoltageMeasure();
    g_pmu.enableBattVoltageMeasure();
    g_pmu.enableSystemVoltageMeasure();
    g_pmu.enableTemperatureMeasure();
    g_pmu.disableTSPinMeasure();
    g_pmu.clearIrqStatus();
    g_pmu_ready = true;
    ESP_LOGI(TAG, "AXP2101 PMU ready");
    return ESP_OK;
}

PowerStatus read_power_status_locked()
{
    PowerStatus status;
    if (ensure_pmu_ready() != ESP_OK) {
        return status;
    }

    status.available = true;
    status.battery_present = g_pmu.isBatteryConnect();
    status.charging = g_pmu.isCharging();
    status.external_power_present = g_pmu.isVbusIn();
    status.battery_voltage_mv = g_pmu.getBattVoltage();
    status.system_voltage_mv = g_pmu.getSystemVoltage();
    status.pmu_temperature_c = g_pmu.getTemperature();
    if (status.battery_present) {
        status.battery_percent = clamp_percent(g_pmu.getBatteryPercent());
    }
    return status;
}

WifiError map_disconnect_reason(wifi_err_reason_t reason)
{
    switch (reason) {
    case WIFI_REASON_NO_AP_FOUND:
    case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
    case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD:
    case WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD:
        return WifiError::NotFound;
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
        return WifiError::AuthFailed;
    case WIFI_REASON_BEACON_TIMEOUT:
        return WifiError::Timeout;
    case WIFI_REASON_ASSOC_FAIL:
        return WifiError::AssocFailed;
    default:
        return WifiError::Unknown;
    }
}

const char *wifi_error_text(WifiError error)
{
    switch (error) {
    case WifiError::None:
        return "Ready";
    case WifiError::NotFound:
        return "Network not found";
    case WifiError::AuthFailed:
        return "Password rejected";
    case WifiError::Timeout:
        return "Connection timed out";
    case WifiError::AssocFailed:
        return "Association failed";
    case WifiError::DriverError:
        return "Driver error";
    case WifiError::Unknown:
    default:
        return "Unknown Wi-Fi error";
    }
}

int rssi_to_level(int rssi)
{
    if (rssi >= -60) {
        return 3;
    }
    if (rssi >= -75) {
        return 2;
    }
    if (rssi >= -88) {
        return 1;
    }
    return 0;
}

bool parse_two_digits(const std::string &text, size_t offset, int &value)
{
    if ((offset + 1) >= text.size()) {
        return false;
    }
    if (!std::isdigit(static_cast<unsigned char>(text[offset])) ||
            !std::isdigit(static_cast<unsigned char>(text[offset + 1]))) {
        return false;
    }
    value = (text[offset] - '0') * 10 + (text[offset + 1] - '0');
    return true;
}

bool parse_four_digits(const std::string &text, int &value)
{
    if (text.size() < 4) {
        return false;
    }
    for (size_t i = 0; i < 4; ++i) {
        if (!std::isdigit(static_cast<unsigned char>(text[i]))) {
            return false;
        }
    }
    value = std::atoi(text.substr(0, 4).c_str());
    return true;
}

bool parse_manual_time(const std::string &date_text, const std::string &time_text, struct tm &timeinfo)
{
    if ((date_text.size() != 10) || (date_text[4] != '-') || (date_text[7] != '-')) {
        return false;
    }
    if ((time_text.size() != 5) || (time_text[2] != ':')) {
        return false;
    }

    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    if (!parse_four_digits(date_text, year) ||
            !parse_two_digits(date_text, 5, month) ||
            !parse_two_digits(date_text, 8, day) ||
            !parse_two_digits(time_text, 0, hour) ||
            !parse_two_digits(time_text, 3, minute)) {
        return false;
    }

    if ((year < 2024) || (month < 1) || (month > 12) || (day < 1) || (day > 31) || (hour > 23) || (minute > 59)) {
        return false;
    }

    timeinfo = {};
    timeinfo.tm_year = year - 1900;
    timeinfo.tm_mon = month - 1;
    timeinfo.tm_mday = day;
    timeinfo.tm_hour = hour;
    timeinfo.tm_min = minute;
    timeinfo.tm_sec = 0;
    timeinfo.tm_isdst = -1;
    return true;
}

std::string format_time(const char *format)
{
    time_t now = 0;
    struct tm timeinfo = {};
    char buffer[32] = {0};

    time(&now);
    localtime_r(&now, &timeinfo);
    std::strftime(buffer, sizeof(buffer), format, &timeinfo);
    return buffer;
}

} // namespace

SettingsService &SettingsService::instance()
{
    static SettingsService service;
    return service;
}

esp_err_t SettingsService::begin()
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_started) {
        return ESP_OK;
    }

    esp_err_t ret = nvs_flash_init();
    if ((ret == ESP_ERR_NVS_NO_FREE_PAGES) || (ret == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "Erase NVS failed");
        ret = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "Init NVS failed");
    ESP_RETURN_ON_ERROR(openStorageLocked(), TAG, "Open settings storage failed");

    setenv("TZ", DEFAULT_TIME_ZONE, 1);
    tzset();
    loadSettingsLocked();
    ESP_RETURN_ON_ERROR(bsp_display_brightness_set(_brightness), TAG, "Apply brightness failed");
    if (ensure_pmu_ready() != ESP_OK) {
        ESP_LOGW(TAG, "AXP2101 PMU is unavailable, battery info will be hidden");
    }

    ESP_LOGI(TAG, "Settings service started: brightness=%d volume=%d auto_sync=%d saved_ssid=%s",
             _brightness, _volume, _auto_time_sync, _saved_ssid.empty() ? "<none>" : _saved_ssid.c_str());
    _started = true;
    return ESP_OK;
}

esp_err_t SettingsService::openStorageLocked()
{
    if (_nvs != 0) {
        return ESP_OK;
    }
    return nvs_open(NVS_NAMESPACE, NVS_READWRITE, &_nvs);
}

void SettingsService::loadSettingsLocked()
{
    _brightness = clamp_percent(loadIntLocked(KEY_BRIGHTNESS, DEFAULT_BRIGHTNESS));
    _volume = clamp_percent(loadIntLocked(KEY_VOLUME, DEFAULT_VOLUME));
    _auto_time_sync = (loadIntLocked(KEY_AUTO_SYNC, 1) != 0);
    _saved_ssid = loadStringLocked(KEY_WIFI_SSID);
    _saved_password = loadStringLocked(KEY_WIFI_PASS);
}

int32_t SettingsService::loadIntLocked(const char *key, int32_t fallback) const
{
    if (_nvs == 0) {
        return fallback;
    }

    int32_t value = fallback;
    if (nvs_get_i32(_nvs, key, &value) != ESP_OK) {
        return fallback;
    }
    return value;
}

std::string SettingsService::loadStringLocked(const char *key) const
{
    if (_nvs == 0) {
        return {};
    }

    size_t required_size = 0;
    if ((nvs_get_str(_nvs, key, nullptr, &required_size) != ESP_OK) || (required_size <= 1)) {
        return {};
    }

    std::string value(required_size - 1, '\0');
    if (nvs_get_str(_nvs, key, value.data(), &required_size) != ESP_OK) {
        return {};
    }
    return value;
}

esp_err_t SettingsService::saveIntLocked(const char *key, int32_t value)
{
    ESP_RETURN_ON_FALSE(_nvs != 0, ESP_ERR_INVALID_STATE, TAG, "NVS is not open");
    ESP_RETURN_ON_ERROR(nvs_set_i32(_nvs, key, value), TAG, "Save integer failed");
    return nvs_commit(_nvs);
}

esp_err_t SettingsService::saveStringLocked(const char *key, const std::string &value)
{
    ESP_RETURN_ON_FALSE(_nvs != 0, ESP_ERR_INVALID_STATE, TAG, "NVS is not open");
    ESP_RETURN_ON_ERROR(nvs_set_str(_nvs, key, value.c_str()), TAG, "Save string failed");
    return nvs_commit(_nvs);
}

esp_err_t SettingsService::prepareNetworkBaseLocked()
{
    if (_network_ready) {
        return ESP_OK;
    }

    esp_err_t ret = esp_netif_init();
    if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
        return ret;
    }

    ret = esp_event_loop_create_default();
    if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
        return ret;
    }

    _sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (_sta_netif == nullptr) {
        _sta_netif = esp_netif_create_default_wifi_sta();
    }
    ESP_RETURN_ON_FALSE(_sta_netif != nullptr, ESP_FAIL, TAG, "Create default STA netif failed");

    _network_ready = true;
    return ESP_OK;
}

esp_err_t SettingsService::ensureWifiStarted()
{
    std::lock_guard<std::mutex> lock(_mutex);
    ESP_RETURN_ON_FALSE(_started, ESP_ERR_INVALID_STATE, TAG, "Service has not started");
    return ensureWifiStartedLocked();
}

esp_err_t SettingsService::ensureWifiStartedLocked()
{
    if (_wifi_ready) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(prepareNetworkBaseLocked(), TAG, "Prepare Wi-Fi base failed");

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.nvs_enable = 0;
    esp_err_t ret = esp_wifi_init(&cfg);
    if ((ret != ESP_OK) && (ret != ESP_ERR_WIFI_INIT_STATE)) {
        _wifi_state = WifiState::Error;
        _wifi_error = WifiError::DriverError;
        return ret;
    }

    if (_wifi_handler == nullptr) {
        ESP_RETURN_ON_ERROR(
            esp_event_handler_instance_register(
                WIFI_EVENT, ESP_EVENT_ANY_ID, &SettingsService::onWifiEvent, this, &_wifi_handler
            ),
            TAG, "Register Wi-Fi event handler failed"
        );
    }
    if (_ip_handler == nullptr) {
        ESP_RETURN_ON_ERROR(
            esp_event_handler_instance_register(
                IP_EVENT, IP_EVENT_STA_GOT_IP, &SettingsService::onIpEvent, this, &_ip_handler
            ),
            TAG, "Register IP event handler failed"
        );
    }

    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "Set Wi-Fi storage failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "Set Wi-Fi mode failed");

    if (!_saved_ssid.empty()) {
        ESP_LOGI(TAG, "Wi-Fi STA ready, restoring saved SSID: %s", _saved_ssid.c_str());
        wifi_config_t wifi_config = {};
        strlcpy(reinterpret_cast<char *>(wifi_config.sta.ssid), _saved_ssid.c_str(), sizeof(wifi_config.sta.ssid));
        strlcpy(
            reinterpret_cast<char *>(wifi_config.sta.password), _saved_password.c_str(), sizeof(wifi_config.sta.password)
        );
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        wifi_config.sta.pmf_cfg.capable = true;
        wifi_config.sta.pmf_cfg.required = false;
        ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG, "Restore Wi-Fi config failed");
        _connect_ssid = _saved_ssid;
        _connect_password = _saved_password;
        _wifi_state = WifiState::Connecting;
    } else {
        _wifi_state = WifiState::Ready;
        ESP_LOGI(TAG, "Wi-Fi STA ready without saved credentials");
    }

    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Start Wi-Fi failed");
    _wifi_ready = true;
    _wifi_error = WifiError::None;
    return ESP_OK;
}

int SettingsService::getBrightness() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _brightness;
}

esp_err_t SettingsService::setBrightness(int value, bool persist)
{
    value = clamp_percent(value);

    std::lock_guard<std::mutex> lock(_mutex);
    _brightness = value;
    ESP_RETURN_ON_ERROR(bsp_display_brightness_set(value), TAG, "Set brightness failed");
    return persist ? saveIntLocked(KEY_BRIGHTNESS, value) : ESP_OK;
}

esp_err_t SettingsService::initAudioLocked()
{
    if (_audio_ready) {
        return ESP_OK;
    }

    _speaker = bsp_audio_codec_speaker_init();
    ESP_RETURN_ON_FALSE(_speaker != nullptr, ESP_FAIL, TAG, "Create speaker codec failed");
    ESP_RETURN_ON_FALSE(
        esp_codec_dev_open(_speaker, const_cast<esp_codec_dev_sample_info_t *>(&SPEAKER_SAMPLE_INFO)) == ESP_CODEC_DEV_OK,
        ESP_FAIL, TAG, "Open speaker codec failed"
    );
    ESP_RETURN_ON_FALSE(esp_codec_dev_set_out_vol(_speaker, _volume) == ESP_CODEC_DEV_OK, ESP_FAIL, TAG,
                        "Apply initial volume failed");

    _audio_ready = true;
    return ESP_OK;
}

int SettingsService::getVolume() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _volume;
}

esp_err_t SettingsService::setVolume(int value, bool persist)
{
    value = clamp_percent(value);

    std::lock_guard<std::mutex> lock(_mutex);
    ESP_RETURN_ON_ERROR(initAudioLocked(), TAG, "Init audio failed");
    ESP_RETURN_ON_FALSE(esp_codec_dev_set_out_vol(_speaker, value) == ESP_CODEC_DEV_OK, ESP_FAIL, TAG,
                        "Set speaker volume failed");
    _volume = value;
    return persist ? saveIntLocked(KEY_VOLUME, value) : ESP_OK;
}

bool SettingsService::getAutoTimeSync() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _auto_time_sync;
}

esp_err_t SettingsService::setAutoTimeSync(bool enable, bool persist)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _auto_time_sync = enable;
    if (!enable) {
        stopTimeSyncLocked();
    } else if (_wifi_state == WifiState::Connected) {
        ESP_RETURN_ON_ERROR(startTimeSyncLocked(), TAG, "Start SNTP failed");
    }

    return persist ? saveIntLocked(KEY_AUTO_SYNC, enable ? 1 : 0) : ESP_OK;
}

esp_err_t SettingsService::startTimeSyncLocked()
{
    if (_time_sync_started) {
        return ESP_OK;
    }

    setenv("TZ", DEFAULT_TIME_ZONE, 1);
    tzset();

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&config);
    _time_sync_started = true;
    ESP_LOGI(TAG, "SNTP started with server pool.ntp.org");
    return ESP_OK;
}

void SettingsService::stopTimeSyncLocked()
{
    if (!_time_sync_started) {
        return;
    }
    esp_netif_sntp_deinit();
    _time_sync_started = false;
}

bool SettingsService::isTimeSynced() const
{
    time_t now = 0;
    time(&now);
    return now >= VALID_TIME_EPOCH;
}

std::string SettingsService::getCurrentDateText() const
{
    if (!isTimeSynced()) {
        return "---- -- --";
    }
    return format_time("%Y-%m-%d");
}

std::string SettingsService::getCurrentTimeText() const
{
    if (!isTimeSynced()) {
        return "--:--";
    }
    return format_time("%H:%M");
}

esp_err_t SettingsService::setManualDateTime(const std::string &date_yyyy_mm_dd, const std::string &time_hh_mm)
{
    std::lock_guard<std::mutex> lock(_mutex);
    ESP_RETURN_ON_FALSE(!_auto_time_sync, ESP_ERR_INVALID_STATE, TAG, "Disable auto sync before manual time set");

    struct tm timeinfo = {};
    ESP_RETURN_ON_FALSE(parse_manual_time(date_yyyy_mm_dd, time_hh_mm, timeinfo), ESP_ERR_INVALID_ARG, TAG,
                        "Invalid date or time format");

    setenv("TZ", DEFAULT_TIME_ZONE, 1);
    tzset();
    time_t epoch = mktime(&timeinfo);
    ESP_RETURN_ON_FALSE(epoch >= VALID_TIME_EPOCH, ESP_ERR_INVALID_ARG, TAG, "Parsed time is not valid");

    struct timeval tv = {
        .tv_sec = epoch,
        .tv_usec = 0,
    };
    ESP_RETURN_ON_FALSE(settimeofday(&tv, nullptr) == 0, ESP_FAIL, TAG, "settimeofday failed");
    ESP_LOGI(TAG, "Manual time set to %s %s", date_yyyy_mm_dd.c_str(), time_hh_mm.c_str());
    return ESP_OK;
}

esp_err_t SettingsService::startWifiScan()
{
    std::lock_guard<std::mutex> lock(_mutex);
    ESP_RETURN_ON_FALSE(_started, ESP_ERR_INVALID_STATE, TAG, "Service has not started");
    ESP_RETURN_ON_ERROR(ensureWifiStartedLocked(), TAG, "Wi-Fi is not ready");
    ESP_RETURN_ON_FALSE(_wifi_state != WifiState::Scanning, ESP_ERR_INVALID_STATE, TAG, "Scan is already running");
    ESP_RETURN_ON_FALSE(_wifi_state != WifiState::Connecting, ESP_ERR_INVALID_STATE, TAG, "Wait for current connection");

    wifi_scan_config_t scan_config = {};
    ESP_LOGI(TAG, "Starting Wi-Fi scan");
    ESP_RETURN_ON_ERROR(esp_wifi_scan_start(&scan_config, false), TAG, "Start Wi-Fi scan failed");
    _wifi_state = WifiState::Scanning;
    _wifi_error = WifiError::None;
    _scan_results.clear();
    return ESP_OK;
}

std::vector<WifiNetwork> SettingsService::getWifiScanResults() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _scan_results;
}

esp_err_t SettingsService::connectWifi(const std::string &ssid, const std::string &password, bool persist)
{
    std::lock_guard<std::mutex> lock(_mutex);
    ESP_RETURN_ON_FALSE(_started, ESP_ERR_INVALID_STATE, TAG, "Service has not started");
    ESP_RETURN_ON_FALSE(!ssid.empty(), ESP_ERR_INVALID_ARG, TAG, "SSID is empty");
    ESP_RETURN_ON_ERROR(ensureWifiStartedLocked(), TAG, "Wi-Fi is not ready");

    wifi_config_t wifi_config = {};
    strlcpy(reinterpret_cast<char *>(wifi_config.sta.ssid), ssid.c_str(), sizeof(wifi_config.sta.ssid));
    strlcpy(reinterpret_cast<char *>(wifi_config.sta.password), password.c_str(), sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = password.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    esp_err_t ret = esp_wifi_disconnect();
    if ((ret != ESP_OK) && (ret != ESP_ERR_WIFI_NOT_CONNECT)) {
        ESP_RETURN_ON_ERROR(ret, TAG, "Disconnect previous Wi-Fi failed");
    }
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG, "Set Wi-Fi config failed");
    ESP_LOGI(TAG, "Connecting to SSID: %s", ssid.c_str());
    ESP_RETURN_ON_ERROR(esp_wifi_connect(), TAG, "Start Wi-Fi connection failed");

    _connect_ssid = ssid;
    _connect_password = password;
    _persist_candidate_credentials = persist;
    _connected_ssid.clear();
    _wifi_retry_count = 0;
    _wifi_disconnect_reason = 0;
    _wifi_error = WifiError::None;
    _wifi_state = WifiState::Connecting;
    return ESP_OK;
}

WifiState SettingsService::getWifiState() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _wifi_state;
}

WifiError SettingsService::getWifiError() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _wifi_error;
}

std::string SettingsService::getWifiStatusText() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    switch (_wifi_state) {
    case WifiState::Idle:
        return "Wi-Fi not started";
    case WifiState::Ready:
        return "Ready to scan or connect";
    case WifiState::Scanning:
        return "Scanning nearby networks...";
    case WifiState::Connecting:
        return _connect_ssid.empty() ? "Connecting..." : ("Connecting to " + _connect_ssid + "...");
    case WifiState::Connected:
        return _connected_ssid.empty() ? "Connected" : ("Connected to " + _connected_ssid);
    case WifiState::Error:
    default:
        return std::string(wifi_error_text(_wifi_error));
    }
}

std::string SettingsService::getConnectedSsid() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _connected_ssid;
}

void SettingsService::updateSignalStrengthLocked()
{
    wifi_ap_record_t access_point = {};
    if (esp_wifi_sta_get_ap_info(&access_point) == ESP_OK) {
        _wifi_signal_rssi = access_point.rssi;
    }
}

int SettingsService::getWifiSignalLevel() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_wifi_state != WifiState::Connected) {
        return 0;
    }
    return rssi_to_level(_wifi_signal_rssi);
}

bool SettingsService::isWifiReady() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _wifi_ready;
}

bool SettingsService::isWifiConnected() const
{
    return getWifiState() == WifiState::Connected;
}

bool SettingsService::isWifiScanning() const
{
    return getWifiState() == WifiState::Scanning;
}

bool SettingsService::isWifiConnecting() const
{
    return getWifiState() == WifiState::Connecting;
}

PowerStatus SettingsService::getPowerStatus() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return read_power_status_locked();
}

std::vector<DeviceInfoEntry> SettingsService::getDeviceInfoEntries() const
{
    std::lock_guard<std::mutex> lock(_mutex);

    std::vector<DeviceInfoEntry> entries;
    entries.reserve(11);

    const esp_app_desc_t *app_desc = esp_app_get_description();
    uint32_t flash_size = 0;
    uint8_t mac[6] = {0};
    PowerStatus power = read_power_status_locked();

    if (esp_flash_get_size(nullptr, &flash_size) != ESP_OK) {
        flash_size = 0;
    }
    if (esp_efuse_mac_get_default(mac) != ESP_OK) {
        std::memset(mac, 0, sizeof(mac));
    }

    entries.push_back({"Chip", build_chip_summary()});
    entries.push_back({"Firmware", (app_desc != nullptr) ? std::string(app_desc->version) : std::string("Unknown")});
    entries.push_back({"ESP-IDF", esp_get_idf_version()});
    entries.push_back({"Flash", format_storage_size(flash_size)});
    entries.push_back({"PSRAM", format_storage_size(esp_psram_get_size())});
    entries.push_back({"MAC", format_mac_address(mac)});
    entries.push_back({
        "Battery",
        !power.available ? std::string("Unavailable") :
        (!power.battery_present ? std::string("Not detected") : (std::to_string(power.battery_percent) + "%"))
    });
    entries.push_back({
        "Battery Volt",
        !power.available ? std::string("Unavailable") :
        (!power.battery_present ? std::string("Not detected") : format_voltage(power.battery_voltage_mv))
    });
    entries.push_back({
        "Charging",
        !power.available ? std::string("Unavailable") :
        (power.charging ? std::string("Charging") :
            (power.external_power_present ? std::string("USB power connected") : std::string("Running on battery")))
    });
    entries.push_back({"System Volt", power.available ? format_voltage(power.system_voltage_mv) : std::string("Unavailable")});
    entries.push_back({"PMU Temp", power.available ? format_temperature(power.pmu_temperature_c) : std::string("Unavailable")});
    return entries;
}

void SettingsService::collectScanResultsLocked()
{
    uint16_t ap_count = 0;
    if (esp_wifi_scan_get_ap_num(&ap_count) != ESP_OK) {
        _wifi_state = _connected_ssid.empty() ? WifiState::Ready : WifiState::Connected;
        _wifi_error = WifiError::DriverError;
        return;
    }

    std::vector<wifi_ap_record_t> records(ap_count);
    if (!records.empty()) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_scan_get_ap_records(&ap_count, records.data()));
    }

    std::sort(records.begin(), records.end(), [](const wifi_ap_record_t &lhs, const wifi_ap_record_t &rhs) {
        return lhs.rssi > rhs.rssi;
    });

    _scan_results.clear();
    _scan_results.reserve(std::min<uint16_t>(ap_count, WIFI_SCAN_RESULT_LIMIT));
    for (const auto &record : records) {
        if (record.ssid[0] == '\0') {
            continue;
        }

        WifiNetwork network;
        network.ssid = reinterpret_cast<const char *>(record.ssid);
        network.rssi = record.rssi;
        network.auth_mode = record.authmode;
        network.channel = record.primary;
        _scan_results.push_back(network);

        if (_scan_results.size() >= WIFI_SCAN_RESULT_LIMIT) {
            break;
        }
    }

    _wifi_state = _connected_ssid.empty() ? WifiState::Ready : WifiState::Connected;
    _wifi_error = WifiError::None;
    ESP_LOGI(TAG, "Wi-Fi scan completed, %u visible APs retained", static_cast<unsigned>(_scan_results.size()));
}

void SettingsService::onWifiEvent(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    auto *self = static_cast<SettingsService *>(arg);
    if ((self == nullptr) || (event_base != WIFI_EVENT)) {
        return;
    }

    std::lock_guard<std::mutex> lock(self->_mutex);
    switch (event_id) {
    case WIFI_EVENT_STA_START:
        if (!self->_connect_ssid.empty()) {
            ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_connect());
            self->_wifi_state = WifiState::Connecting;
        }
        break;
    case WIFI_EVENT_SCAN_DONE:
        self->collectScanResultsLocked();
        break;
    case WIFI_EVENT_STA_DISCONNECTED: {
        auto *event = static_cast<wifi_event_sta_disconnected_t *>(event_data);
        self->_connected_ssid.clear();
        self->_wifi_signal_rssi = -100;
        self->_wifi_disconnect_reason = (event != nullptr) ? event->reason : 0;
        self->stopTimeSyncLocked();

        if (!self->_connect_ssid.empty() && (self->_wifi_retry_count < WIFI_RETRY_LIMIT)) {
            self->_wifi_retry_count++;
            ESP_LOGW(TAG, "Wi-Fi disconnected from %s, reason=%d retry=%d/%d", self->_connect_ssid.c_str(),
                     self->_wifi_disconnect_reason, self->_wifi_retry_count, WIFI_RETRY_LIMIT);
            self->_wifi_state = WifiState::Connecting;
            ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_connect());
        } else if (!self->_connect_ssid.empty()) {
            ESP_LOGE(TAG, "Wi-Fi connect failed for %s, reason=%d", self->_connect_ssid.c_str(), self->_wifi_disconnect_reason);
            self->_wifi_state = WifiState::Error;
            self->_wifi_error = map_disconnect_reason(static_cast<wifi_err_reason_t>(self->_wifi_disconnect_reason));
            self->_connect_ssid.clear();
            self->_connect_password.clear();
            self->_persist_candidate_credentials = false;
        } else {
            self->_wifi_state = WifiState::Ready;
        }
        break;
    }
    default:
        break;
    }
}

void SettingsService::onIpEvent(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)event_data;

    auto *self = static_cast<SettingsService *>(arg);
    if ((self == nullptr) || (event_base != IP_EVENT) || (event_id != IP_EVENT_STA_GOT_IP)) {
        return;
    }

    std::lock_guard<std::mutex> lock(self->_mutex);
    self->_wifi_state = WifiState::Connected;
    self->_wifi_error = WifiError::None;
    self->_wifi_retry_count = 0;
    self->_connected_ssid = self->_connect_ssid;
    self->updateSignalStrengthLocked();
    ESP_LOGI(TAG, "Wi-Fi connected: ssid=%s rssi=%d", self->_connected_ssid.c_str(), self->_wifi_signal_rssi);

    if (self->_persist_candidate_credentials) {
        self->_saved_ssid = self->_connect_ssid;
        self->_saved_password = self->_connect_password;
        ESP_ERROR_CHECK_WITHOUT_ABORT(self->saveStringLocked(KEY_WIFI_SSID, self->_saved_ssid));
        ESP_ERROR_CHECK_WITHOUT_ABORT(self->saveStringLocked(KEY_WIFI_PASS, self->_saved_password));
        self->_persist_candidate_credentials = false;
    }

    if (self->_auto_time_sync) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(self->startTimeSyncLocked());
    }
}

} // namespace storybook











