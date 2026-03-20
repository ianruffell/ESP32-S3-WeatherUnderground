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
    WeatherAPI(const char* stationId, const char* apiKey);
    bool fetchWeatherData(WeatherData& data);
    bool isConnected();
    void setCredentials(const String& stationId, const String& apiKey);

private:
    String stationId;
    String apiKey;
    bool fetchFromWunderground(WeatherData& data);
    bool fetchFromProWeatherLive(WeatherData& data);
};

#endif
