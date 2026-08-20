#ifndef WEATHER_H
#define WEATHER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

struct WeatherData {
    float temperature;
    float humidity;
    float pressure;
    float windSpeed;
    float windGust;
    float windDirection;
    float rainToday;
    float rainRate;
    float dewPoint;
    float uvIndex;
    float solarRadiation;
    float feelTemp;
    float latitude;
    float longitude;
    int utcOffsetMinutes;
    int pressureTrend;
    int lastUpdate;
    bool isValid;
};

class WeatherAPI {
public:
    WeatherAPI(const char* provider, const char* stationId, const char* credential);
    bool fetchWeatherData(WeatherData& data);
    bool isConnected();
    void setCredentials(const String& provider, const String& stationId, const String& credential);

private:
    String provider;
    String stationId;
    String credential;
    bool fetchFromWunderground(WeatherData& data);
    bool fetchFromProWeatherLive(WeatherData& data);

    // A station's internal device id never changes, so resolving it once saves
    // a request on every refresh. Cleared if the metrics call later fails.
    String cachedDeviceStationId;
    String cachedDeviceId;
    float cachedLatitude = 0.0f;
    float cachedLongitude = 0.0f;
    int cachedUtcOffsetMinutes = 0;
};

#endif
