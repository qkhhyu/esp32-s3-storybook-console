#include "suite_settings_service.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <ctime>
#include <utility>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_heap_caps.h"
#include "esp_check.h"
#include "esp_sntp.h"
#include "bsp/display.h"
#include "bsp_board_extra.h"

namespace suite {

namespace {

constexpr const char *TAG = "SuiteSettings";
constexpr const char *NVS_NAMESPACE = "suite_cfg";
constexpr const char *KEY_BRIGHTNESS = "brightness";
constexpr const char *KEY_VOLUME = "volume";
constexpr const char *KEY_LANGUAGE = "language";
constexpr const char *KEY_AUTO_SYNC = "auto_sync";
constexpr const char *KEY_WIFI_SSID = "wifi_ssid";
constexpr const char *KEY_WIFI_PASS = "wifi_pass";
constexpr int DEFAULT_BRIGHTNESS = 82;
constexpr int DEFAULT_VOLUME = 60;
constexpr time_t VALID_TIME_EPOCH = 1704067200; // 2024-01-01 00:00:00 UTC
constexpr int WIFI_STATIC_RX_BUF_NUM = 4;
constexpr int WIFI_DYNAMIC_RX_BUF_NUM = 8;
constexpr int WIFI_STATIC_TX_BUF_NUM = 8;
constexpr int WIFI_DYNAMIC_TX_BUF_NUM = 8;
constexpr int WIFI_RX_MGMT_BUF_NUM = 4;
constexpr int WIFI_CACHE_TX_BUF_NUM = 2;
constexpr int WIFI_MGMT_SBUF_NUM_CFG = 6;
constexpr int TIME_SYNC_INITIAL_DELAY_MS = 5000;
constexpr int TIME_SYNC_RETRY_DELAY_MS = 2000;
constexpr int TIME_SYNC_RETRY_COUNT = 4;
constexpr size_t TIME_SYNC_MIN_INTERNAL_FREE = 48 * 1024;
constexpr size_t TIME_SYNC_MIN_LARGEST_BLOCK = 20 * 1024;

int clampBrightness(int value)
{
    return std::clamp(value, BSP_LCD_BACKLIGHT_BRIGHTNESS_MIN, BSP_LCD_BACKLIGHT_BRIGHTNESS_MAX);
}

int clampVolume(int value)
{
    return std::clamp(value, 0, 100);
}

int rssiToLevel(int rssi)
{
    if (rssi >= -60) {
        return 3;
    }
    if (rssi >= -75) {
        return 2;
    }
    if (rssi >= -90) {
        return 1;
    }
    return 0;
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
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "Init NVS failed");
    ESP_RETURN_ON_ERROR(openStorage(), TAG, "Open settings storage failed");

    loadSettings();
    bsp_display_brightness_set(_brightness);
    ret = prepareNetworkBaseLocked();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Network base init deferred: %s", esp_err_to_name(ret));
    }

    _started = true;
    return ESP_OK;
}

esp_err_t SettingsService::ensureWifiStarted()
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_started) {
        return ESP_ERR_INVALID_STATE;
    }
    return initWifiLocked();
}

bool SettingsService::isWifiReady() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _wifi_ready;
}

esp_err_t SettingsService::openStorage()
{
    if (_nvs != 0) {
        return ESP_OK;
    }
    return nvs_open(NVS_NAMESPACE, NVS_READWRITE, &_nvs);
}

void SettingsService::loadSettings()
{
    _brightness = clampBrightness(loadInt(KEY_BRIGHTNESS, DEFAULT_BRIGHTNESS));
    _volume = clampVolume(loadInt(KEY_VOLUME, DEFAULT_VOLUME));
    _language = (loadInt(KEY_LANGUAGE, static_cast<int32_t>(UiLanguage::Chinese)) == static_cast<int32_t>(UiLanguage::English))
                    ? UiLanguage::English
                    : UiLanguage::Chinese;
    _auto_time_sync = (loadInt(KEY_AUTO_SYNC, 1) != 0);
    _saved_ssid = loadString(KEY_WIFI_SSID);
    _saved_password = loadString(KEY_WIFI_PASS);
}

int32_t SettingsService::loadInt(const char *key, int32_t default_value) const
{
    int32_t value = default_value;
    if ((_nvs == 0) || (nvs_get_i32(_nvs, key, &value) != ESP_OK)) {
        return default_value;
    }
    return value;
}

std::string SettingsService::loadString(const char *key) const
{
    if (_nvs == 0) {
        return {};
    }

    size_t required_size = 0;
    if (nvs_get_str(_nvs, key, nullptr, &required_size) != ESP_OK || (required_size == 0)) {
        return {};
    }

    std::string value(required_size - 1, '\0');
    if (nvs_get_str(_nvs, key, value.data(), &required_size) != ESP_OK) {
        return {};
    }
    return value;
}

esp_err_t SettingsService::saveInt(const char *key, int32_t value)
{
    if (_nvs == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    ESP_RETURN_ON_ERROR(nvs_set_i32(_nvs, key, value), TAG, "Save integer failed");
    return nvs_commit(_nvs);
}

esp_err_t SettingsService::saveString(const char *key, const std::string &value)
{
    if (_nvs == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    ESP_RETURN_ON_ERROR(nvs_set_str(_nvs, key, value.c_str()), TAG, "Save string failed");
    return nvs_commit(_nvs);
}

esp_err_t SettingsService::prepareNetworkBaseLocked()
{
    if (_network_base_ready) {
        return ESP_OK;
    }

    esp_err_t ret = esp_netif_init();
    if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
        _last_wifi_error = ret;
        return ret;
    }

    ret = esp_event_loop_create_default();
    if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
        _last_wifi_error = ret;
        return ret;
    }

    _sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (_sta_netif == nullptr) {
        _sta_netif = esp_netif_create_default_wifi_sta();
    }
    if (_sta_netif == nullptr) {
        _last_wifi_error = ESP_FAIL;
        return ESP_FAIL;
    }

    _network_base_ready = true;
    _last_wifi_error = ESP_OK;
    return ESP_OK;
}

esp_err_t SettingsService::initWifi()
{
    std::lock_guard<std::mutex> lock(_mutex);
    return initWifiLocked();
}

esp_err_t SettingsService::initWifiLocked()
{
    if (_wifi_ready) {
        return ESP_OK;
    }

    esp_err_t ret = prepareNetworkBaseLocked();
    if (ret != ESP_OK) {
        _wifi_init_failed = true;
        return ret;
    }

    ESP_LOGI(
        TAG, "Wi-Fi init heap: free=%u internal=%u largest_internal=%u",
        static_cast<unsigned>(esp_get_free_heap_size()),
        static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
        static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL))
    );

    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    wifi_init_cfg.nvs_enable = 0;
    wifi_init_cfg.static_rx_buf_num = WIFI_STATIC_RX_BUF_NUM;
    wifi_init_cfg.dynamic_rx_buf_num = WIFI_DYNAMIC_RX_BUF_NUM;
    wifi_init_cfg.static_tx_buf_num = WIFI_STATIC_TX_BUF_NUM;
    wifi_init_cfg.dynamic_tx_buf_num = WIFI_DYNAMIC_TX_BUF_NUM;
    wifi_init_cfg.rx_mgmt_buf_num = WIFI_RX_MGMT_BUF_NUM;
    wifi_init_cfg.cache_tx_buf_num = WIFI_CACHE_TX_BUF_NUM;
    wifi_init_cfg.mgmt_sbuf_num = WIFI_MGMT_SBUF_NUM_CFG;
    ret = esp_wifi_init(&wifi_init_cfg);
    if ((ret != ESP_OK) && (ret != ESP_ERR_WIFI_INIT_STATE)) {
        ESP_LOGW(
            TAG, "esp_wifi_init failed: %s (free=%u internal=%u largest_internal=%u)", esp_err_to_name(ret),
            static_cast<unsigned>(esp_get_free_heap_size()),
            static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
            static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL))
        );
        _last_wifi_error = ret;
        _wifi_init_failed = true;
        return ret;
    }

    if (_wifi_handler == nullptr) {
        ESP_RETURN_ON_ERROR(
            esp_event_handler_instance_register(
                WIFI_EVENT, ESP_EVENT_ANY_ID, &SettingsService::wifiEventHandler, this, &_wifi_handler
            ),
            TAG, "Register Wi-Fi event failed"
        );
    }
    if (_ip_handler == nullptr) {
        ESP_RETURN_ON_ERROR(
            esp_event_handler_instance_register(
                IP_EVENT, IP_EVENT_STA_GOT_IP, &SettingsService::ipEventHandler, this, &_ip_handler
            ),
            TAG, "Register IP event failed"
        );
    }

    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "Set Wi-Fi storage failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "Set Wi-Fi mode failed");

    ret = esp_wifi_start();
    if ((ret != ESP_OK) && (ret != ESP_ERR_WIFI_CONN)) {
        _last_wifi_error = ret;
        _wifi_init_failed = true;
        return ret;
    }

    _wifi_ready = true;
    _wifi_init_failed = false;
    _last_wifi_error = ESP_OK;
    if (!_saved_ssid.empty()) {
        wifi_config_t wifi_cfg = {};
        strlcpy(reinterpret_cast<char *>(wifi_cfg.sta.ssid), _saved_ssid.c_str(), sizeof(wifi_cfg.sta.ssid));
        strlcpy(reinterpret_cast<char *>(wifi_cfg.sta.password), _saved_password.c_str(), sizeof(wifi_cfg.sta.password));
        wifi_cfg.sta.pmf_cfg.capable = true;
        wifi_cfg.sta.pmf_cfg.required = false;

        ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg), TAG, "Restore Wi-Fi config failed");
        ret = esp_wifi_connect();
        if ((ret != ESP_OK) && (ret != ESP_ERR_WIFI_CONN)) {
            _last_wifi_error = ret;
            return ret;
        }
        _wifi_connecting = true;
    }
    return ESP_OK;
}

int SettingsService::getBrightness() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _brightness;
}

esp_err_t SettingsService::setBrightness(int value, bool persist)
{
    value = clampBrightness(value);

    std::lock_guard<std::mutex> lock(_mutex);
    _brightness = value;
    ESP_RETURN_ON_ERROR(bsp_display_brightness_set(value), TAG, "Set brightness failed");
    return persist ? saveInt(KEY_BRIGHTNESS, value) : ESP_OK;
}

int SettingsService::getVolume() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _volume;
}

esp_err_t SettingsService::setVolume(int value, bool persist)
{
    value = clampVolume(value);

    std::lock_guard<std::mutex> lock(_mutex);
    _volume = value;
    ESP_RETURN_ON_ERROR(bsp_extra_codec_init(), TAG, "Init codec failed");
    ESP_RETURN_ON_ERROR(bsp_extra_codec_volume_set(value, nullptr), TAG, "Set volume failed");
    return persist ? saveInt(KEY_VOLUME, value) : ESP_OK;
}

UiLanguage SettingsService::getLanguage() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _language;
}

esp_err_t SettingsService::setLanguage(UiLanguage language, bool persist)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _language = language;
    return persist ? saveInt(KEY_LANGUAGE, static_cast<int32_t>(language)) : ESP_OK;
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
    if (_wifi_connected) {
        if (enable) {
            scheduleTimeSyncLocked();
        } else {
            stopTimeSyncLocked();
        }
    }
    return persist ? saveInt(KEY_AUTO_SYNC, enable ? 1 : 0) : ESP_OK;
}

bool SettingsService::isTimeSynced() const
{
    time_t now = 0;
    time(&now);
    return now >= VALID_TIME_EPOCH;
}

esp_err_t SettingsService::startWifiScan()
{
    ESP_RETURN_ON_ERROR(ensureWifiStarted(), TAG, "Wi-Fi init failed");

    wifi_scan_config_t scan_cfg = {};
    bool stop_connecting = false;
    bool should_resume_connection = false;

    {
        std::lock_guard<std::mutex> lock(_mutex);
        ESP_RETURN_ON_FALSE(!_wifi_scanning, ESP_ERR_INVALID_STATE, TAG, "Scan already in progress");

        stop_connecting = _wifi_connecting;
        should_resume_connection = (_wifi_connected || _wifi_connecting) && !_saved_ssid.empty();

        _suspend_auto_reconnect = true;
        _resume_connection_after_scan = should_resume_connection;
        _wifi_scanning = true;
        _scan_results.clear();
        _last_wifi_error = ESP_OK;
        if (stop_connecting) {
            _disconnect_requested = true;
            _wifi_connecting = false;
        }
    }

    if (stop_connecting) {
        esp_err_t disconnect_ret = esp_wifi_disconnect();
        if ((disconnect_ret != ESP_OK) && (disconnect_ret != ESP_ERR_WIFI_NOT_CONNECT) &&
            (disconnect_ret != ESP_ERR_WIFI_CONN)) {
            std::lock_guard<std::mutex> lock(_mutex);
            _wifi_scanning = false;
            _suspend_auto_reconnect = false;
            _resume_connection_after_scan = false;
            _disconnect_requested = false;
            _last_wifi_error = disconnect_ret;
            return disconnect_ret;
        }

        // Scanning loses to an active connection attempt. Give the driver a brief
        // moment to process the disconnect before starting the scan.
        vTaskDelay(pdMS_TO_TICKS(120));
    }

    ESP_LOGI(
        TAG, "Starting Wi-Fi scan (free=%u internal=%u psram=%u)",
        static_cast<unsigned>(esp_get_free_heap_size()),
        static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
        static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM))
    );
    esp_err_t ret = esp_wifi_scan_start(&scan_cfg, false);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Start Wi-Fi scan failed: %s", esp_err_to_name(ret));

        bool reconnect_after_failure = false;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _wifi_scanning = false;
            _suspend_auto_reconnect = false;
            reconnect_after_failure = _resume_connection_after_scan && !_saved_ssid.empty();
            _resume_connection_after_scan = false;
            _last_wifi_error = ret;
        }

        if (reconnect_after_failure) {
            esp_err_t reconnect_ret = esp_wifi_connect();
            if ((reconnect_ret == ESP_OK) || (reconnect_ret == ESP_ERR_WIFI_CONN)) {
                std::lock_guard<std::mutex> lock(_mutex);
                _wifi_connecting = true;
            } else {
                std::lock_guard<std::mutex> lock(_mutex);
                _last_wifi_error = reconnect_ret;
            }
        }
        return ret;
    }

    return ESP_OK;
}

void SettingsService::finishWifiScan()
{
    std::vector<WifiNetwork> results;
    uint16_t ap_count = 0;
    esp_err_t ret = esp_wifi_scan_get_ap_num(&ap_count);
    if (ret == ESP_OK) {
        std::vector<wifi_ap_record_t> records(ap_count);
        if (!records.empty()) {
            ret = esp_wifi_scan_get_ap_records(&ap_count, records.data());
        }
        if (ret == ESP_OK) {
            std::sort(records.begin(), records.end(), [](const wifi_ap_record_t &lhs, const wifi_ap_record_t &rhs) {
                return lhs.rssi > rhs.rssi;
            });

            for (const auto &record : records) {
                if (record.ssid[0] == '\0') {
                    continue;
                }
                WifiNetwork network;
                network.ssid = reinterpret_cast<const char *>(record.ssid);
                network.rssi = record.rssi;
                network.is_open = (record.authmode == WIFI_AUTH_OPEN);
                results.push_back(network);
                if (results.size() >= 10) {
                    break;
                }
            }
        }
    }
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Collect Wi-Fi scan results failed: %s", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "Wi-Fi scan finished, %u networks", static_cast<unsigned>(results.size()));

    bool should_resume_connection = false;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _scan_results = std::move(results);
        _wifi_scanning = false;
        _last_wifi_error = ret;
        should_resume_connection = _resume_connection_after_scan && !_saved_ssid.empty() && !_wifi_connected;
        _resume_connection_after_scan = false;
        _suspend_auto_reconnect = false;
        if (should_resume_connection) {
            _wifi_connecting = true;
        }
    }

    if (should_resume_connection) {
        esp_err_t connect_ret = esp_wifi_connect();
        if ((connect_ret != ESP_OK) && (connect_ret != ESP_ERR_WIFI_CONN)) {
            std::lock_guard<std::mutex> lock(_mutex);
            _wifi_connecting = false;
            _last_wifi_error = connect_ret;
        }
    }
}

bool SettingsService::isWifiScanning() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _wifi_scanning;
}

std::vector<WifiNetwork> SettingsService::getWifiScanResults() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _scan_results;
}

esp_err_t SettingsService::connectWifi(const std::string &ssid, const std::string &password, bool persist)
{
    ESP_RETURN_ON_ERROR(ensureWifiStarted(), TAG, "Wi-Fi init failed");

    std::lock_guard<std::mutex> lock(_mutex);
    ESP_RETURN_ON_FALSE(!ssid.empty(), ESP_ERR_INVALID_ARG, TAG, "Empty SSID");

    ESP_LOGI(
        TAG, "Connecting to SSID '%s' (password_len=%u persist=%d free=%u internal=%u largest_internal=%u)",
        ssid.c_str(), static_cast<unsigned>(password.size()), persist ? 1 : 0,
        static_cast<unsigned>(esp_get_free_heap_size()),
        static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
        static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL))
    );

    wifi_config_t wifi_cfg = {};
    strlcpy(reinterpret_cast<char *>(wifi_cfg.sta.ssid), ssid.c_str(), sizeof(wifi_cfg.sta.ssid));
    strlcpy(reinterpret_cast<char *>(wifi_cfg.sta.password), password.c_str(), sizeof(wifi_cfg.sta.password));
    wifi_cfg.sta.pmf_cfg.capable = true;
    wifi_cfg.sta.pmf_cfg.required = false;

    _disconnect_requested = true;
    _suspend_auto_reconnect = false;
    _resume_connection_after_scan = false;

    esp_err_t ret = esp_wifi_disconnect();
    if ((ret != ESP_OK) && (ret != ESP_ERR_WIFI_NOT_CONNECT) && (ret != ESP_ERR_WIFI_CONN)) {
        _disconnect_requested = false;
        ESP_RETURN_ON_ERROR(ret, TAG, "Disconnect existing Wi-Fi failed");
    }
    ESP_LOGI(TAG, "esp_wifi_disconnect -> %s", esp_err_to_name(ret));
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg), TAG, "Set Wi-Fi config failed");
    ESP_LOGI(TAG, "esp_wifi_set_config done for '%s'", ssid.c_str());

    _saved_ssid = ssid;
    _saved_password = password;
    _wifi_connecting = true;
    _wifi_connected = false;
    _connected_ssid.clear();
    _last_wifi_error = ESP_OK;

    if (persist) {
        ESP_RETURN_ON_ERROR(saveString(KEY_WIFI_SSID, ssid), TAG, "Save SSID failed");
        ESP_LOGI(TAG, "Saved SSID for '%s'", ssid.c_str());
        ESP_RETURN_ON_ERROR(saveString(KEY_WIFI_PASS, password), TAG, "Save password failed");
        ESP_LOGI(TAG, "Saved password for '%s'", ssid.c_str());
    }

    ret = esp_wifi_connect();
    ESP_LOGI(TAG, "esp_wifi_connect -> %s", esp_err_to_name(ret));
    return ret;
}

bool SettingsService::isWifiConnected() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _wifi_connected;
}

bool SettingsService::isWifiConnecting() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _wifi_connecting;
}

std::string SettingsService::getConnectedSsid() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _connected_ssid;
}

void SettingsService::updateWifiSignalLocked()
{
    wifi_ap_record_t ap_info = {};
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        _last_rssi = ap_info.rssi;
    }
}

int SettingsService::getWifiSignalLevel() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_wifi_connected) {
        return 0;
    }

    wifi_ap_record_t ap_info = {};
    int rssi = _last_rssi;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        rssi = ap_info.rssi;
    }
    return rssiToLevel(rssi);
}

esp_err_t SettingsService::getLastWifiError() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _last_wifi_error;
}

void SettingsService::scheduleTimeSyncLocked()
{
    if (_time_sync_started || _time_sync_pending || !_auto_time_sync || !_wifi_connected) {
        return;
    }

    _time_sync_pending = true;
    BaseType_t ok = xTaskCreateWithCaps(
        &SettingsService::deferredTimeSyncTask,
        "time_sync",
        4096,
        this,
        3,
        nullptr,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );
    if (ok != pdPASS) {
        _time_sync_pending = false;
        ESP_LOGW(TAG, "Create deferred time sync task failed");
    }
}

void SettingsService::deferredTimeSyncTask(void *arg)
{
    auto *self = static_cast<SettingsService *>(arg);
    if (self == nullptr) {
        vTaskDelete(nullptr);
        return;
    }

    for (int attempt = 0; attempt < TIME_SYNC_RETRY_COUNT; ++attempt) {
        vTaskDelay(pdMS_TO_TICKS((attempt == 0) ? TIME_SYNC_INITIAL_DELAY_MS : TIME_SYNC_RETRY_DELAY_MS));

        std::lock_guard<std::mutex> lock(self->_mutex);
        if (!self->_auto_time_sync || !self->_wifi_connected) {
            self->_time_sync_pending = false;
            vTaskDelete(nullptr);
            return;
        }

        size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        size_t largest_internal = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
        if ((internal_free >= TIME_SYNC_MIN_INTERNAL_FREE) && (largest_internal >= TIME_SYNC_MIN_LARGEST_BLOCK)) {
            self->startTimeSyncLocked();
            self->_time_sync_pending = false;
            vTaskDelete(nullptr);
            return;
        }

        ESP_LOGW(
            TAG,
            "Delay time sync due to low internal heap (attempt=%d free=%u largest=%u)",
            attempt + 1,
            static_cast<unsigned>(internal_free),
            static_cast<unsigned>(largest_internal)
        );
    }

    {
        std::lock_guard<std::mutex> lock(self->_mutex);
        self->_time_sync_pending = false;
    }
    ESP_LOGW(TAG, "Skip time sync because internal heap stayed too low");
    vTaskDelete(nullptr);
}

void SettingsService::startTimeSyncLocked()
{
    if (_time_sync_started) {
        esp_sntp_restart();
        return;
    }

    setenv("TZ", "CST-8", 1);
    tzset();
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp.aliyun.com");
    esp_sntp_setservername(1, "pool.ntp.org");
    esp_sntp_setservername(2, "time.cloudflare.com");
    esp_sntp_init();
    _time_sync_started = true;
}

void SettingsService::stopTimeSyncLocked()
{
    if (!_time_sync_started) {
        return;
    }
    esp_sntp_stop();
    _time_sync_started = false;
    _time_sync_pending = false;
}

void SettingsService::wifiEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)event_base;
    (void)event_data;
    auto *self = static_cast<SettingsService *>(arg);
    if (self == nullptr) {
        return;
    }

    switch (event_id) {
    case WIFI_EVENT_STA_START:
        break;
    case WIFI_EVENT_STA_CONNECTED: {
        std::lock_guard<std::mutex> lock(self->_mutex);
        self->_wifi_connecting = false;
        self->_wifi_connected = true;
        self->_connected_ssid = self->_saved_ssid;
        self->updateWifiSignalLocked();
        break;
    }
    case WIFI_EVENT_SCAN_DONE:
        self->finishWifiScan();
        break;
    case WIFI_EVENT_STA_DISCONNECTED: {
        std::lock_guard<std::mutex> lock(self->_mutex);
        self->_wifi_connected = false;
        self->_wifi_connecting = false;
        self->_last_rssi = -100;
        self->_connected_ssid.clear();
        self->stopTimeSyncLocked();
        bool disconnect_requested = self->_disconnect_requested;
        self->_disconnect_requested = false;
        if (!disconnect_requested && !self->_suspend_auto_reconnect && !self->_saved_ssid.empty()) {
            esp_wifi_connect();
            self->_wifi_connecting = true;
        }
        break;
    }
    default:
        break;
    }
}

void SettingsService::ipEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)event_base;
    (void)event_id;
    (void)event_data;

    auto *self = static_cast<SettingsService *>(arg);
    if (self == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(self->_mutex);
    self->_wifi_connected = true;
    self->_wifi_connecting = false;
    self->_connected_ssid = self->_saved_ssid;
    self->updateWifiSignalLocked();
    if (self->_auto_time_sync) {
        self->scheduleTimeSyncLocked();
    }
}

} // namespace suite

