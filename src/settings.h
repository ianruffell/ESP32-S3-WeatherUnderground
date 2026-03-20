#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>

constexpr uint8_t MAX_WEATHER_PAGES = 5;

struct WeatherPageSettings {
    String name;
    String stationId;
    String timeZone;
    float latitude;
    float longitude;
};

struct DeviceSettings {
    String deviceName;
    String wifiSsid;
    String wifiPassword;
    String weatherApiKey;
    WeatherPageSettings weatherPages[MAX_WEATHER_PAGES];
    uint8_t weatherPageCount;
    String timeZone;
    String ntpServer1;
    String ntpServer2;
    float locationLat;
    float locationLon;

    bool hasWiFiCredentials() const;
    bool hasWeatherCredentials() const;
    uint8_t configuredWeatherPageCount() const;
    String weatherPageTitle(uint8_t index) const;
    void sanitizeWeatherPages();
};

class SettingsStore {
public:
    void begin();
    bool load(DeviceSettings& settings);
    bool save(const DeviceSettings& settings);
    void reset();
    static DeviceSettings defaults();
};

#endif
