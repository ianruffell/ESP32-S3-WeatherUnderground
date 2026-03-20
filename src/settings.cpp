#include "settings.h"
#include "config.h"
#include <Preferences.h>

namespace {
Preferences preferences;

String readString(const char* key, const String& fallback) {
    return preferences.getString(key, fallback);
}

void clearWeatherPage(WeatherPageSettings& page) {
    page.name = "";
    page.stationId = "";
    page.timeZone = "";
    page.latitude = LOCATION_LAT;
    page.longitude = LOCATION_LON;
}
}

bool DeviceSettings::hasWiFiCredentials() const {
    return !wifiSsid.isEmpty();
}

bool DeviceSettings::hasWeatherCredentials() const {
    return !weatherApiKey.isEmpty() && configuredWeatherPageCount() > 0;
}

uint8_t DeviceSettings::configuredWeatherPageCount() const {
    uint8_t limit = weatherPageCount;
    if (limit == 0 || limit > MAX_WEATHER_PAGES) {
        limit = MAX_WEATHER_PAGES;
    }

    uint8_t count = 0;
    for (uint8_t i = 0; i < limit; ++i) {
        if (!weatherPages[i].stationId.isEmpty()) {
            ++count;
        }
    }

    return count;
}

String DeviceSettings::weatherPageTitle(uint8_t index) const {
    if (index >= MAX_WEATHER_PAGES) {
        return "";
    }

    if (!weatherPages[index].name.isEmpty()) {
        return weatherPages[index].name;
    }

    if (!weatherPages[index].stationId.isEmpty()) {
        return weatherPages[index].stationId;
    }

    String fallback = "Location ";
    fallback += String(index + 1);
    return fallback;
}

void DeviceSettings::sanitizeWeatherPages() {
    WeatherPageSettings compact[MAX_WEATHER_PAGES];
    uint8_t compactCount = 0;

    for (uint8_t i = 0; i < MAX_WEATHER_PAGES; ++i) {
        compact[i] = {};
    }

    for (uint8_t i = 0; i < MAX_WEATHER_PAGES; ++i) {
        weatherPages[i].name.trim();
        weatherPages[i].stationId.trim();
        if (weatherPages[i].stationId.isEmpty()) {
            continue;
        }

        compact[compactCount] = weatherPages[i];
        if (compact[compactCount].name.isEmpty()) {
            compact[compactCount].name = "Location ";
            compact[compactCount].name += String(compactCount + 1);
        }
        if (compact[compactCount].timeZone.isEmpty()) {
            compact[compactCount].timeZone = timeZone;
        }
        ++compactCount;
        if (compactCount >= MAX_WEATHER_PAGES) {
            break;
        }
    }

    for (uint8_t i = 0; i < MAX_WEATHER_PAGES; ++i) {
        clearWeatherPage(weatherPages[i]);
    }

    if (compactCount == 0) {
        weatherPageCount = 1;
        weatherPages[0].name = "Location 1";
        return;
    }

    weatherPageCount = compactCount;
    for (uint8_t i = 0; i < compactCount; ++i) {
        weatherPages[i] = compact[i];
    }
}

void SettingsStore::begin() {
    preferences.begin(SETTINGS_NAMESPACE, false);
}

bool SettingsStore::load(DeviceSettings& settings) {
    settings = defaults();

    uint32_t version = preferences.getUInt("version", 0);
    if (version != SETTINGS_VERSION) {
        return false;
    }

    settings.deviceName = readString("device_name", settings.deviceName);
    settings.wifiSsid = readString("wifi_ssid", settings.wifiSsid);
    settings.wifiPassword = readString("wifi_pass", settings.wifiPassword);
    settings.weatherApiKey = readString("api_key", settings.weatherApiKey);
    settings.weatherPageCount = preferences.getUChar("page_count", settings.weatherPageCount);
    settings.timeZone = readString("time_zone", settings.timeZone);
    settings.ntpServer1 = readString("ntp_1", settings.ntpServer1);
    settings.ntpServer2 = readString("ntp_2", settings.ntpServer2);
    settings.locationLat = preferences.getFloat("lat", settings.locationLat);
    settings.locationLon = preferences.getFloat("lon", settings.locationLon);

    if (settings.weatherPageCount == 0 || settings.weatherPageCount > MAX_WEATHER_PAGES) {
        String legacyStationId = readString("station_id", "");
        settings.weatherPageCount = 1;
        settings.weatherPages[0].name = "Location 1";
        settings.weatherPages[0].stationId = legacyStationId;
        settings.weatherPages[0].timeZone = settings.timeZone;
        settings.weatherPages[0].latitude = settings.locationLat;
        settings.weatherPages[0].longitude = settings.locationLon;
    } else {
        for (uint8_t i = 0; i < settings.weatherPageCount; ++i) {
            char key[20];
            snprintf(key, sizeof(key), "page_name_%u", i);
            settings.weatherPages[i].name = readString(key, settings.weatherPages[i].name);
            snprintf(key, sizeof(key), "page_station_%u", i);
            settings.weatherPages[i].stationId = readString(key, settings.weatherPages[i].stationId);
            snprintf(key, sizeof(key), "page_tz_%u", i);
            settings.weatherPages[i].timeZone = readString(key, settings.timeZone);
            snprintf(key, sizeof(key), "page_lat_%u", i);
            settings.weatherPages[i].latitude = preferences.getFloat(key, settings.locationLat);
            snprintf(key, sizeof(key), "page_lon_%u", i);
            settings.weatherPages[i].longitude = preferences.getFloat(key, settings.locationLon);
        }
    }

    settings.sanitizeWeatherPages();
    return true;
}

bool SettingsStore::save(const DeviceSettings& settings) {
    DeviceSettings normalized = settings;
    normalized.sanitizeWeatherPages();

    bool ok = true;
    ok &= preferences.putUInt("version", SETTINGS_VERSION) > 0;
    ok &= preferences.putString("device_name", normalized.deviceName) > 0;
    ok &= preferences.putString("wifi_ssid", normalized.wifiSsid) >= 0;
    ok &= preferences.putString("wifi_pass", normalized.wifiPassword) >= 0;
    ok &= preferences.putString("api_key", normalized.weatherApiKey) >= 0;
    ok &= preferences.putUChar("page_count", normalized.weatherPageCount) > 0;
    ok &= preferences.putString("time_zone", normalized.timeZone) >= 0;
    ok &= preferences.putString("ntp_1", normalized.ntpServer1) >= 0;
    ok &= preferences.putString("ntp_2", normalized.ntpServer2) >= 0;
    ok &= preferences.putFloat("lat", normalized.locationLat) > 0;
    ok &= preferences.putFloat("lon", normalized.locationLon) > 0;

    const String primaryStationId = normalized.weatherPageCount > 0 ? normalized.weatherPages[0].stationId : "";
    ok &= preferences.putString("station_id", primaryStationId) >= 0;

    for (uint8_t i = 0; i < MAX_WEATHER_PAGES; ++i) {
        char key[20];
        snprintf(key, sizeof(key), "page_name_%u", i);
        if (i < normalized.weatherPageCount) {
            ok &= preferences.putString(key, normalized.weatherPages[i].name) >= 0;
        } else {
            preferences.remove(key);
        }

        snprintf(key, sizeof(key), "page_station_%u", i);
        if (i < normalized.weatherPageCount) {
            ok &= preferences.putString(key, normalized.weatherPages[i].stationId) >= 0;
        } else {
            preferences.remove(key);
        }

        snprintf(key, sizeof(key), "page_tz_%u", i);
        if (i < normalized.weatherPageCount) {
            ok &= preferences.putString(key, normalized.weatherPages[i].timeZone) >= 0;
        } else {
            preferences.remove(key);
        }

        snprintf(key, sizeof(key), "page_lat_%u", i);
        if (i < normalized.weatherPageCount) {
            ok &= preferences.putFloat(key, normalized.weatherPages[i].latitude) > 0;
        } else {
            preferences.remove(key);
        }

        snprintf(key, sizeof(key), "page_lon_%u", i);
        if (i < normalized.weatherPageCount) {
            ok &= preferences.putFloat(key, normalized.weatherPages[i].longitude) > 0;
        } else {
            preferences.remove(key);
        }
    }

    return ok;
}

void SettingsStore::reset() {
    preferences.clear();
}

DeviceSettings SettingsStore::defaults() {
    DeviceSettings settings;
    settings.deviceName = DEFAULT_DEVICE_NAME;
    settings.wifiSsid = WIFI_SSID;
    settings.wifiPassword = WIFI_PASSWORD;
    settings.weatherApiKey = WEATHER_API_KEY;
    settings.weatherPageCount = 1;
    settings.weatherPages[0].name = "Location 1";
    settings.weatherPages[0].stationId = WEATHER_STATION_ID;
    settings.weatherPages[0].timeZone = TIME_ZONE;
    settings.weatherPages[0].latitude = LOCATION_LAT;
    settings.weatherPages[0].longitude = LOCATION_LON;
    settings.timeZone = TIME_ZONE;
    settings.ntpServer1 = NTP_SERVER_1;
    settings.ntpServer2 = NTP_SERVER_2;
    settings.locationLat = LOCATION_LAT;
    settings.locationLon = LOCATION_LON;
    settings.sanitizeWeatherPages();
    return settings;
}
