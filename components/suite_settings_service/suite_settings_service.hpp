#pragma once

#include <mutex>
#include <string>
#include <vector>
#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs.h"

namespace suite {

enum class UiLanguage : uint8_t {
    Chinese = 0,
    English = 1,
};

struct WifiNetwork {
    std::string ssid;
    int rssi = -100;
    bool is_open = false;
};

class SettingsService {
public:
    static SettingsService &instance();

    esp_err_t begin();
    esp_err_t ensureWifiStarted();
    bool isWifiReady() const;

    int getBrightness() const;
    esp_err_t setBrightness(int value, bool persist = true);

    int getVolume() const;
    esp_err_t setVolume(int value, bool persist = true);

    UiLanguage getLanguage() const;
    esp_err_t setLanguage(UiLanguage language, bool persist = true);

    bool getAutoTimeSync() const;
    esp_err_t setAutoTimeSync(bool enable, bool persist = true);
    bool isTimeSynced() const;

    esp_err_t startWifiScan();
    bool isWifiScanning() const;
    std::vector<WifiNetwork> getWifiScanResults() const;

    esp_err_t connectWifi(const std::string &ssid, const std::string &password, bool persist = true);
    bool isWifiConnected() const;
    bool isWifiConnecting() const;
    std::string getConnectedSsid() const;
    int getWifiSignalLevel() const;
    esp_err_t getLastWifiError() const;

private:
    SettingsService() = default;
    SettingsService(const SettingsService &) = delete;
    SettingsService &operator=(const SettingsService &) = delete;

    esp_err_t openStorage();
    void loadSettings();
    esp_err_t saveInt(const char *key, int32_t value);
    esp_err_t saveString(const char *key, const std::string &value);
    std::string loadString(const char *key) const;
    int32_t loadInt(const char *key, int32_t default_value) const;

    esp_err_t prepareNetworkBaseLocked();
    esp_err_t initWifi();
    esp_err_t initWifiLocked();
    void scheduleTimeSyncLocked();
    void startTimeSyncLocked();
    void stopTimeSyncLocked();
    void updateWifiSignalLocked();
    void finishWifiScan();

    static void deferredTimeSyncTask(void *arg);
    static void wifiEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
    static void ipEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

    mutable std::mutex _mutex;
    bool _started = false;
    bool _network_base_ready = false;
    bool _wifi_ready = false;
    bool _wifi_init_failed = false;
    bool _wifi_scanning = false;
    bool _wifi_connecting = false;
    bool _wifi_connected = false;
    bool _suspend_auto_reconnect = false;
    bool _resume_connection_after_scan = false;
    bool _disconnect_requested = false;
    bool _time_sync_started = false;
    bool _time_sync_pending = false;
    bool _auto_time_sync = true;
    UiLanguage _language = UiLanguage::Chinese;
    int _brightness = 82;
    int _volume = 60;
    int _last_rssi = -100;
    esp_err_t _last_wifi_error = ESP_OK;
    nvs_handle_t _nvs = 0;
    esp_netif_t *_sta_netif = nullptr;
    esp_event_handler_instance_t _wifi_handler = nullptr;
    esp_event_handler_instance_t _ip_handler = nullptr;
    std::string _saved_ssid;
    std::string _saved_password;
    std::string _connected_ssid;
    std::vector<WifiNetwork> _scan_results;
};

} // namespace suite

