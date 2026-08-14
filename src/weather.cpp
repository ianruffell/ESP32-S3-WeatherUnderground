#include "weather.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <vector>

namespace {
constexpr const char* PRO_WEATHER_LIVE_API = "https://proweatherlive.net/api/";

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

String urlEncode(const String& value) {
    static const char hex[] = "0123456789ABCDEF";
    String encoded;
    encoded.reserve(value.length() * 3);
    for (size_t i = 0; i < value.length(); ++i) {
        const uint8_t c = static_cast<uint8_t>(value.charAt(i));
        if ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += static_cast<char>(c);
        } else {
            encoded += '%';
            encoded += hex[c >> 4];
            encoded += hex[c & 0x0f];
        }
    }
    return encoded;
}

int authenticatedGet(const String& path, const String& token, String& body) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    const String url = String(PRO_WEATHER_LIVE_API) + path;
    if (!http.begin(client, url)) {
        Serial.println("Failed to initialise ProWeatherLive HTTPS request.");
        return -1;
    }

    http.setReuse(false);
    http.setTimeout(15000);
    http.setUserAgent("ESP32-S3-Weather-Display/1.0");
    http.addHeader("Accept", "application/json, application/msgpack");
    http.addHeader("Authorization", "Bearer " + token);

    const int httpCode = http.GET();
    body = http.getString();
    http.end();
    return httpCode;
}

bool appendByteArray(JsonArrayConst values, std::vector<uint8_t>& bytes) {
    if (values.isNull() || values.size() == 0) {
        return false;
    }

    bytes.reserve(values.size());
    for (JsonVariantConst value : values) {
        if (!value.is<int>()) {
            return false;
        }
        bytes.push_back(static_cast<uint8_t>(value.as<int>()));
    }
    return !bytes.empty();
}

bool decodeProWeatherLivePayload(const String& body, JsonDocument& decoded) {
    if (body.isEmpty()) {
        return false;
    }

    const char first = body.charAt(0);
    if (first != '{' && first != '[') {
        DeserializationError error = deserializeMsgPack(
            decoded,
            reinterpret_cast<const uint8_t*>(body.c_str()),
            body.length()
        );
        return !error;
    }

    JsonDocument wrapper;
    DeserializationError jsonError = deserializeJson(wrapper, body);
    if (jsonError) {
        Serial.print("ProWeatherLive JSON parse error: ");
        Serial.println(jsonError.c_str());
        return false;
    }

    JsonArrayConst direct = wrapper.as<JsonArrayConst>();
    if (!direct.isNull() && direct.size() == 2 &&
        direct[0].is<JsonArrayConst>() && direct[1].is<JsonArrayConst>()) {
        decoded.set(wrapper);
        return true;
    }

    JsonVariantConst bufferValue = wrapper.as<JsonVariantConst>();
    if (bufferValue.is<JsonObjectConst>()) {
        JsonObjectConst object = bufferValue.as<JsonObjectConst>();
        if (object["data"].is<JsonObjectConst>()) {
            bufferValue = object["data"];
        }
    }

    JsonArrayConst byteArray;
    if (bufferValue.is<JsonObjectConst>()) {
        byteArray = bufferValue["data"].as<JsonArrayConst>();
    } else {
        byteArray = bufferValue.as<JsonArrayConst>();
    }

    std::vector<uint8_t> bytes;
    if (!appendByteArray(byteArray, bytes)) {
        Serial.println("ProWeatherLive response did not contain a MessagePack buffer.");
        return false;
    }

    DeserializationError msgPackError = deserializeMsgPack(decoded, bytes.data(), bytes.size());
    if (msgPackError) {
        Serial.print("ProWeatherLive MessagePack parse error: ");
        Serial.println(msgPackError.c_str());
        return false;
    }
    return true;
}

float metricNumber(JsonArrayConst headers,
                   JsonArrayConst row,
                   const char* const* aliases,
                   size_t aliasCount,
                   float fallback = 0.0f) {
    for (size_t aliasIndex = 0; aliasIndex < aliasCount; ++aliasIndex) {
        size_t valueIndex = 0;
        for (JsonVariantConst header : headers) {
            const char* key = header.as<const char*>();
            if (key != nullptr && strcmp(key, aliases[aliasIndex]) == 0 && valueIndex < row.size()) {
                JsonVariantConst value = row[valueIndex];
                if (value.is<float>() || value.is<double>() || value.is<long>() || value.is<int>()) {
                    return value.as<float>();
                }
            }
            ++valueIndex;
        }
    }
    return fallback;
}

String objectString(JsonObjectConst object, const char* primary, const char* fallback = nullptr) {
    const char* value = object[primary] | static_cast<const char*>(nullptr);
    if (value == nullptr && fallback != nullptr) {
        value = object[fallback] | static_cast<const char*>(nullptr);
    }
    return value == nullptr ? String() : String(value);
}

float objectNumber(JsonObjectConst object, const char* primary, const char* fallback = nullptr) {
    JsonVariantConst value = object[primary];
    if (value.isNull() && fallback != nullptr) {
        value = object[fallback];
    }
    return value | 0.0f;
}

int normalizeOffsetMinutes(float offset) {
    const float absoluteOffset = fabs(offset);
    if (absoluteOffset <= 24.0f) {
        return static_cast<int>(roundf(offset * 60.0f));
    }
    if (absoluteOffset > 1440.0f) {
        return static_cast<int>(roundf(offset / 60.0f));
    }
    return static_cast<int>(roundf(offset));
}

int currentOffsetForTimeZone(const String& timeZone) {
    const char* posixTimeZone = nullptr;
    if (timeZone == "Europe/London") {
        posixTimeZone = "GMT0BST,M3.5.0/1,M10.5.0/2";
    }
    if (posixTimeZone == nullptr) {
        return 0;
    }

    const char* previous = getenv("TZ");
    const String previousValue = previous == nullptr ? String() : String(previous);
    setenv("TZ", posixTimeZone, 1);
    tzset();

    time_t now = time(nullptr);
    struct tm localTime;
    localtime_r(&now, &localTime);
    char offsetText[8] = {};
    strftime(offsetText, sizeof(offsetText), "%z", &localTime);

    if (previous == nullptr) {
        unsetenv("TZ");
    } else {
        setenv("TZ", previousValue.c_str(), 1);
    }
    tzset();

    if (strlen(offsetText) != 5) {
        return 0;
    }
    const int sign = offsetText[0] == '-' ? -1 : 1;
    const int hours = (offsetText[1] - '0') * 10 + (offsetText[2] - '0');
    const int minutes = (offsetText[3] - '0') * 10 + (offsetText[4] - '0');
    return sign * (hours * 60 + minutes);
}
}

WeatherAPI::WeatherAPI(const char* provider, const char* stationId, const char* credential)
    : provider(provider == nullptr ? "wunderground" : provider),
      stationId(stationId == nullptr ? "" : stationId),
      credential(credential == nullptr ? "" : credential) {
}

void WeatherAPI::setCredentials(const String& newProvider,
                                const String& newStationId,
                                const String& newCredential) {
    provider = newProvider;
    stationId = newStationId;
    credential = newCredential;
}

bool WeatherAPI::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

bool WeatherAPI::fetchWeatherData(WeatherData& data) {
    if (!isConnected()) {
        data.isValid = false;
        return false;
    }

    if (provider == "proweatherlive") {
        return fetchFromProWeatherLive(data);
    }
    return fetchFromWunderground(data);
}

bool WeatherAPI::fetchFromWunderground(WeatherData& data) {
    if (stationId.isEmpty() || credential.isEmpty()) {
        data.isValid = false;
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = "https://api.weather.com/v2/pws/observations/current?stationId=" + stationId +
                 "&format=json&units=m&numericPrecision=decimal&apiKey=" + credential;

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

bool WeatherAPI::fetchFromProWeatherLive(WeatherData& data) {
    if (stationId.isEmpty() || credential.isEmpty()) {
        data.isValid = false;
        return false;
    }

    String deviceBody;
    const String devicePath = "weatherDevices?wsid=" + urlEncode(stationId) +
                              "&%24limit=1&%24paginate=false";
    const int deviceCode = authenticatedGet(devicePath, credential, deviceBody);
    if (deviceCode != HTTP_CODE_OK) {
        Serial.print("ProWeatherLive device request failed: ");
        Serial.println(deviceCode);
        if (deviceCode == HTTP_CODE_UNAUTHORIZED) {
            Serial.println("The ProWeatherLive access token is missing, expired, or invalid.");
        }
        return false;
    }

    JsonDocument deviceDoc;
    DeserializationError deviceError = deserializeJson(deviceDoc, deviceBody);
    if (deviceError) {
        Serial.print("ProWeatherLive device JSON parse error: ");
        Serial.println(deviceError.c_str());
        return false;
    }

    JsonObjectConst device;
    JsonArrayConst devices = deviceDoc.as<JsonArrayConst>();
    if (!devices.isNull() && devices.size() > 0) {
        device = devices[0].as<JsonObjectConst>();
    } else if (deviceDoc["data"].is<JsonArrayConst>() && deviceDoc["data"].size() > 0) {
        device = deviceDoc["data"][0].as<JsonObjectConst>();
    } else if (deviceDoc.is<JsonObjectConst>() && deviceDoc["_id"].is<const char*>()) {
        device = deviceDoc.as<JsonObjectConst>();
    }

    if (device.isNull()) {
        Serial.println("ProWeatherLive station was not found for this account.");
        return false;
    }

    const String deviceId = objectString(device, "_id", "id");
    if (deviceId.isEmpty()) {
        Serial.println("ProWeatherLive station response did not include a device ID.");
        return false;
    }

    String metricsBody;
    const String metricsPath = "weatherMetrics/batchLastData?devices%5B0%5D=" + urlEncode(deviceId);
    const int metricsCode = authenticatedGet(metricsPath, credential, metricsBody);
    if (metricsCode != HTTP_CODE_OK) {
        Serial.print("ProWeatherLive observation request failed: ");
        Serial.println(metricsCode);
        return false;
    }

    JsonDocument metricsDoc;
    if (!decodeProWeatherLivePayload(metricsBody, metricsDoc)) {
        return false;
    }

    JsonArrayConst payload = metricsDoc.as<JsonArrayConst>();
    if (payload.size() < 2) {
        Serial.println("ProWeatherLive observation payload was incomplete.");
        return false;
    }

    JsonArrayConst headers = payload[0].as<JsonArrayConst>();
    JsonArrayConst rows = payload[1].as<JsonArrayConst>();
    if (headers.isNull() || rows.isNull() || rows.size() == 0) {
        Serial.println("ProWeatherLive did not return a current observation.");
        return false;
    }

    JsonArrayConst row = rows[rows.size() - 1].as<JsonArrayConst>();
    if (row.isNull()) {
        Serial.println("ProWeatherLive current observation had an unexpected format.");
        return false;
    }

    static const char* temperatureKeys[] = {"tempc0", "temperature", "temp"};
    static const char* humidityKeys[] = {"humc0", "humidity", "humi"};
    static const char* pressureKeys[] = {"relbaro", "relbaroi", "absbaro", "baro"};
    static const char* windSpeedKeys[] = {"windspd", "windSpeed", "windspeed"};
    static const char* windGustKeys[] = {"gust", "windgust", "windGust"};
    static const char* windDirectionKeys[] = {"winddir", "windDirection", "winddirection"};
    static const char* rainTodayKeys[] = {"dailyrain", "rainToday", "raindaily", "rainfall"};
    static const char* rainRateKeys[] = {"rainrate", "rainRate"};
    static const char* dewPointKeys[] = {"dewpoint", "dewPoint"};
    static const char* uvKeys[] = {"uvi", "uv", "UV"};
    static const char* solarKeys[] = {"solarrad", "solarRadiation", "solar"};
    static const char* feelsLikeKeys[] = {"feelslike", "heatindex", "windchill"};
    static const char* timeKeys[] = {"time", "timestamp", "updatedAt"};

    data.temperature = metricNumber(headers, row, temperatureKeys, 3);
    data.humidity = metricNumber(headers, row, humidityKeys, 3);
    data.pressure = metricNumber(headers, row, pressureKeys, 4);
    data.windSpeed = metricNumber(headers, row, windSpeedKeys, 3);
    data.windGust = metricNumber(headers, row, windGustKeys, 3);
    data.windDirection = metricNumber(headers, row, windDirectionKeys, 3);
    data.rainToday = metricNumber(headers, row, rainTodayKeys, 4);
    data.rainRate = metricNumber(headers, row, rainRateKeys, 2);
    data.dewPoint = metricNumber(headers, row, dewPointKeys, 2);
    data.uvIndex = metricNumber(headers, row, uvKeys, 3);
    data.solarRadiation = metricNumber(headers, row, solarKeys, 3);
    data.feelTemp = metricNumber(headers, row, feelsLikeKeys, 3, data.temperature);
    data.latitude = objectNumber(device, "latitude", "lat");
    data.longitude = objectNumber(device, "longitude", "lon");

    const float explicitOffset = objectNumber(device, "timezoneOffset", "utcOffset");
    const String timeZone = objectString(device, "timezone", "timeZone");
    data.utcOffsetMinutes = explicitOffset == 0.0f
        ? currentOffsetForTimeZone(timeZone)
        : normalizeOffsetMinutes(explicitOffset);
    data.lastUpdate = static_cast<int>(metricNumber(
        headers,
        row,
        timeKeys,
        3,
        static_cast<float>(millis() / 1000)
    ));
    data.isValid = true;

    Serial.println("ProWeatherLive observation parsed successfully.");
    return true;
}
