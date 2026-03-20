#include "weather.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>

namespace {
bool parseIso8601Utc(const String& text, struct tm& out) {
    memset(&out, 0, sizeof(out));
    return strptime(text.c_str(), "%Y-%m-%dT%H:%M:%SZ", &out) != nullptr;
}

bool parseIso8601Local(const String& text, struct tm& out) {
    memset(&out, 0, sizeof(out));
    return strptime(text.c_str(), "%Y-%m-%d %H:%M:%S", &out) != nullptr;
}

int deriveUtcOffsetMinutes(const String& utcText, const String& localText) {
    struct tm utcTm;
    struct tm localTm;
    if (!parseIso8601Utc(utcText, utcTm) || !parseIso8601Local(localText, localTm)) {
        return 0;
    }

#if defined(__APPLE__) || defined(__unix__)
    time_t utcEpoch = timegm(&utcTm);
    time_t localEpochAssumingUtc = timegm(&localTm);
#else
    char* previousTz = getenv("TZ");
    String previousValue = previousTz == nullptr ? "" : String(previousTz);
    setenv("TZ", "UTC0", 1);
    tzset();
    time_t utcEpoch = mktime(&utcTm);
    time_t localEpochAssumingUtc = mktime(&localTm);
    if (previousTz == nullptr) {
        unsetenv("TZ");
    } else {
        setenv("TZ", previousValue.c_str(), 1);
    }
    tzset();
#endif
    return static_cast<int>(difftime(localEpochAssumingUtc, utcEpoch) / 60.0);
}

JsonObjectConst observationUnits(const JsonObjectConst& observation) {
    JsonObjectConst units = observation["metric"].as<JsonObjectConst>();
    if (!units.isNull()) {
        return units;
    }

    units = observation["imperial"].as<JsonObjectConst>();
    if (!units.isNull()) {
        return units;
    }

    return observation["hybrid"].as<JsonObjectConst>();
}

float observationNumber(const JsonObjectConst& object, const char* primaryKey, const char* fallbackKey = nullptr) {
    JsonVariantConst value = object[primaryKey];
    if (value.isNull() && fallbackKey != nullptr) {
        value = object[fallbackKey];
    }
    return value | 0.0f;
}

void logWundergroundFailure(int httpCode, const String& body) {
    Serial.print("Wunderground request failed: ");
    Serial.println(httpCode);
    if (!body.isEmpty()) {
        Serial.println("Wunderground error body: " + body);
    }
}
}

WeatherAPI::WeatherAPI(const char* stationId, const char* apiKey)
    : stationId(stationId == nullptr ? "" : stationId),
      apiKey(apiKey == nullptr ? "" : apiKey) {
}

void WeatherAPI::setCredentials(const String& newStationId, const String& newApiKey) {
    stationId = newStationId;
    apiKey = newApiKey;
}

bool WeatherAPI::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

bool WeatherAPI::fetchWeatherData(WeatherData& data) {
    if (!isConnected()) {
        data.isValid = false;
        return false;
    }

    return fetchFromWunderground(data);
}

bool WeatherAPI::fetchFromWunderground(WeatherData& data) {
    if (stationId.isEmpty() || apiKey.isEmpty()) {
        data.isValid = false;
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = "https://api.weather.com/v2/pws/observations/current?stationId=" + stationId +
                 "&format=json&units=m&numericPrecision=decimal&apiKey=" + apiKey;

    if (!http.begin(client, url)) {
        Serial.println("Failed to initialise Wunderground HTTPS request.");
        return false;
    }

    http.setReuse(false);
    http.setTimeout(10000);
    http.setUserAgent("ESP32-S3-Weather-Display/1.0");
    http.addHeader("Accept", "application/json");

    int httpCode = http.GET();
    Serial.print("Wunderground HTTP Code: ");
    Serial.println(httpCode);

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        http.end();

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (error) {
            Serial.print("JSON parse error: ");
            Serial.println(error.c_str());
            return false;
        }

        JsonArrayConst observations = doc["observations"].as<JsonArrayConst>();
        if (observations.isNull() || observations.size() == 0) {
            Serial.println("Wunderground response did not include any observations.");
            return false;
        }

        JsonObjectConst observation = observations[0].as<JsonObjectConst>();
        JsonObjectConst units = observationUnits(observation);
        if (units.isNull()) {
            Serial.println("Wunderground response did not include a units payload.");
            return false;
        }

        data.temperature = observationNumber(units, "temp");
        data.humidity = observationNumber(observation, "humidity");
        data.pressure = observationNumber(units, "pressure");
        data.windSpeed = observationNumber(units, "windSpeed");
        data.windGust = observationNumber(units, "windGust");
        data.windDirection = observationNumber(observation, "winddir", "windDir");
        data.rainToday = observationNumber(units, "precipTotal");
        data.rainRate = observationNumber(units, "precipRate");
        data.dewPoint = observationNumber(units, "dewpt");
        data.uvIndex = observationNumber(observation, "uv", "UV");
        data.solarRadiation = observationNumber(observation, "solarRadiation");
        data.feelTemp = observationNumber(units, "heatIndex");
        if (data.feelTemp == 0.0f) {
            data.feelTemp = data.temperature;
        }
        data.latitude = observationNumber(observation, "lat");
        data.longitude = observationNumber(observation, "lon");
        data.utcOffsetMinutes = deriveUtcOffsetMinutes(
            String(static_cast<const char*>(observation["obsTimeUtc"] | "")),
            String(static_cast<const char*>(observation["obsTimeLocal"] | ""))
        );
        data.lastUpdate = millis() / 1000;
        data.isValid = true;

        Serial.println("Wunderground observation parsed successfully.");
        return true;
    }

    String errorBody = http.getString();
    http.end();
    logWundergroundFailure(httpCode, errorBody);
    return false;
}
