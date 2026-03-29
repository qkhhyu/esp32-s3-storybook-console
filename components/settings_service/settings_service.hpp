#pragma once

#include <mutex>
#include <string>
#include <vector>
#include "esp_codec_dev.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs.h"

namespace storybook {

struct WifiNetwork {
    std::string ssid;
    int rssi = -100;
    wifi_auth_mode_t auth_mode = WIFI_AUTH_OPEN;
    uint8_t channel = 0;

    bool isOpen() const
    {
        return auth_mode == WIFI_AUTH_OPEN;
    }
};

struct DeviceInfoEntry {
    std::string label;
    std::string value;
};

struct PowerStatus {
    bool available = false;
    bool battery_present = false;
    bool charging = false;
    bool external_power_present = false;
    int battery_percent = -1;
    uint16_t battery_voltage_mv = 0;
    uint16_t system_voltage_mv = 0;
    float pmu_temperature_c = 0.0F;
};

enum class WifiState : uint8_t {
    Idle = 0,
    Ready,
    Scanning,
    Connecting,
    Connected,
    Error,
};

enum class WifiError : uint8_t {
    None = 0,
    NotFound,
    AuthFailed,
    Timeout,
    AssocFailed,
    DriverError,
    Unknown,
};

class SettingsService {
public:
    static SettingsService &instance();

    esp_err_t begin();
    esp_err_t ensureWifiStarted();

    int getBrightness() const;
    esp_err_t setBrightness(int value, bool persist = true);

    int getVolume() const;
    esp_err_t setVolume(int value, bool persist = true);

    bool getAutoTimeSync() const;
    esp_err_t setAutoTimeSync(bool enable, bool persist = true);
    bool isTimeSynced() const;
    std::string getCurrentDateText() const;
    std::string getCurrentTimeText() const;
    esp_err_t setManualDateTime(const std::string &date_yyyy_mm_dd, const std::string &time_hh_mm);

    esp_err_t startWifiScan();
    std::vector<WifiNetwork> getWifiScanResults() const;

    esp_err_t connectWifi(const std::string &ssid, const std::string &password, bool persist = true);
    WifiState getWifiState() const;
    WifiError getWifiError() const;
    std::string getWifiStatusText() const;
    std::string getConnectedSsid() const;
    int getWifiSignalLevel() const;
    bool isWifiReady() const;
    bool isWifiConnected() const;
    bool isWifiScanning() const;
    bool isWifiConnecting() const;
    PowerStatus getPowerStatus() const;
    std::vector<DeviceInfoEntry> getDeviceInfoEntries() const;

private:
    SettingsService() = default;
    SettingsService(const SettingsService &) = delete;
    SettingsService &operator=(const SettingsService &) = delete;

    esp_err_t openStorageLocked();
    void loadSettingsLocked();
    esp_err_t saveIntLocked(const char *key, int32_t value);
    esp_err_t saveStringLocked(const char *key, const std::string &value);
    int32_t loadIntLocked(const char *key, int32_t fallback) const;
    std::string loadStringLocked(const char *key) const;

    esp_err_t prepareNetworkBaseLocked();
    esp_err_t ensureWifiStartedLocked();
    esp_err_t initAudioLocked();
    esp_err_t startTimeSyncLocked();
    void stopTimeSyncLocked();
    void collectScanResultsLocked();
    void updateSignalStrengthLocked();

    static void onWifiEvent(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
    static void onIpEvent(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

    mutable std::mutex _mutex;
    bool _started = false;
    bool _network_ready = false;
    bool _wifi_ready = false;
    bool _audio_ready = false;
    bool _time_sync_started = false;
    bool _auto_time_sync = true;
    int _brightness = 80;
    int _volume = 60;
    WifiState _wifi_state = WifiState::Idle;
    WifiError _wifi_error = WifiError::None;
    int _wifi_disconnect_reason = 0;
    int _wifi_signal_rssi = -100;
    int _wifi_retry_count = 0;
    nvs_handle_t _nvs = 0;
    esp_netif_t *_sta_netif = nullptr;
    esp_event_handler_instance_t _wifi_handler = nullptr;
    esp_event_handler_instance_t _ip_handler = nullptr;
    esp_codec_dev_handle_t _speaker = nullptr;
    std::string _saved_ssid;
    std::string _saved_password;
    std::string _connect_ssid;
    std::string _connect_password;
    std::string _connected_ssid;
    bool _persist_candidate_credentials = false;
    std::vector<WifiNetwork> _scan_results;
};

} // namespace storybook



