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
};

#endif
