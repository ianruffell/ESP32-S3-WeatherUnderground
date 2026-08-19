#include "aircraft.h"
#include "airlines.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>

// Collects an HTTP body into a fixed buffer so HTTPClient can do the chunked
// decoding for us.
class MemoryStream : public Stream {
public:
    MemoryStream(uint8_t* buffer, size_t capacity) : buffer(buffer), capacity(capacity), length(0) {}

    size_t write(uint8_t value) override {
        if (length >= capacity) {
            return 0;
        }
        buffer[length++] = value;
        return 1;
    }

    size_t write(const uint8_t* data, size_t size) override {
        const size_t room = capacity - length;
        const size_t copied = size < room ? size : room;
        memcpy(buffer + length, data, copied);
        length += copied;
        return copied;
    }

    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}

    size_t size() const { return length; }

private:
    uint8_t* buffer;
    size_t capacity;
    size_t length;
};

static const char* ADSB_HOST = "https://api.adsb.lol";
static const char* LOGO_HOST = "https://images.kiwi.com/airlines/64";
static const char* ROUTE_HOST = "https://api.adsbdb.com";
static const char* ROUTE_FALLBACK_HOST = "https://hexdb.io";
static constexpr size_t MAX_LOGO_BYTES = 32 * 1024;

// Feeds pad callsigns with spaces ("BAW55G  ") and sometimes with '@' or other
// filler when the transponder field is not fully decoded, so keep only the
// characters a callsign or registration can legitimately contain.
static void copy_field(char* dest, size_t size, const char* src, bool allowHyphen = false) {
    if (src == nullptr) {
        dest[0] = '\0';
        return;
    }

    size_t out = 0;
    while (*src != '\0' && out < size - 1) {
        const unsigned char c = static_cast<unsigned char>(*src);
        if (isalnum(c) || (allowHyphen && c == '-')) {
            dest[out++] = static_cast<char>(toupper(c));
        }
        ++src;
    }
    dest[out] = '\0';
}

// Copies free text such as place names, folding Latin-1 accents down to plain
// ASCII: the built-in Montserrat fonts have no accented glyphs, so "Montreal"
// would otherwise render with a blank box in the middle.
static void copy_text(char* dest, size_t size, const char* src) {
    // Base letters for UTF-8 0xC3 0x80..0xBF (Latin-1 supplement).
    static const char FOLDED[] =
        "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTs"
        "aaaaaaaceeeeiiiidnooooo/ouuuuyty";

    if (src == nullptr || size == 0) {
        if (size > 0) {
            dest[0] = '\0';
        }
        return;
    }

    size_t out = 0;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(src); *p != '\0' && out < size - 1; ++p) {
        if (*p < 0x80) {
            dest[out++] = static_cast<char>(*p);
        } else if (*p == 0xC3 && p[1] >= 0x80) {
            dest[out++] = FOLDED[p[1] - 0x80];
            ++p;
        } else if (*p >= 0xC0) {
            // Any other multi-byte sequence: skip it and its continuation bytes.
            while (p[1] >= 0x80 && p[1] < 0xC0) {
                ++p;
            }
        }
    }
    dest[out] = '\0';
}

// Airline callsigns are three letters followed by the flight number; anything
// else (a registration like G-ZBJB) is general aviation.
static bool operator_code_from_callsign(const char* callsign, char* out) {
    out[0] = '\0';
    if (callsign == nullptr || strlen(callsign) < 4) {
        return false;
    }

    for (int i = 0; i < 3; ++i) {
        if (!isalpha(static_cast<unsigned char>(callsign[i]))) {
            return false;
        }
    }

    bool hasDigit = false;
    for (const char* p = callsign + 3; *p != '\0'; ++p) {
        if (isdigit(static_cast<unsigned char>(*p))) {
            hasDigit = true;
            break;
        }
    }
    if (!hasDigit) {
        return false;
    }

    for (int i = 0; i < 3; ++i) {
        out[i] = static_cast<char>(toupper(static_cast<unsigned char>(callsign[i])));
    }
    out[3] = '\0';
    return true;
}

static const AirlineEntry* lookup_airline(const char* icao) {
    if (icao == nullptr || icao[0] == '\0') {
        return nullptr;
    }

    for (size_t i = 0; i < AIRLINE_COUNT; ++i) {
        if (strcmp(AIRLINES[i].icao, icao) == 0) {
            return &AIRLINES[i];
        }
    }
    return nullptr;
}

// Keeps the list ordered by distance, dropping anything past the last slot.
static void insert_by_distance(TrafficData& data, const AircraftInfo& candidate) {
    uint8_t position = data.count;
    while (position > 0 && data.aircraft[position - 1].distanceNm > candidate.distanceNm) {
        if (position < MAX_TRACKED_AIRCRAFT) {
            data.aircraft[position] = data.aircraft[position - 1];
        }
        --position;
    }

    if (position >= MAX_TRACKED_AIRCRAFT) {
        return;
    }

    data.aircraft[position] = candidate;
    if (data.count < MAX_TRACKED_AIRCRAFT) {
        ++data.count;
    }
}

bool AircraftAPI::fetchTraffic(double latitude, double longitude, uint16_t radiusNm, TrafficData& data) {
    // Built up separately and only committed on success: the feed times out
    // occasionally, and a failed fetch must not discard the last good list.
    TrafficData staging = {};
    staging.radiusNm = radiusNm;

    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    char url[128];
    snprintf(
        url,
        sizeof(url),
        "%s/v2/lat/%.4f/lon/%.4f/dist/%u",
        ADSB_HOST,
        latitude,
        longitude,
        static_cast<unsigned>(radiusNm)
    );

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(10000);
    if (!http.begin(client, url)) {
        return false;
    }

    const int status = http.GET();
    if (status != HTTP_CODE_OK) {
        log_i("[traffic] GET failed status=%d heap=%u psram=%u",
              status,
              static_cast<unsigned>(ESP.getFreeHeap()),
              static_cast<unsigned>(ESP.getFreePsram()));
        http.end();
        return false;
    }

    // Only a handful of the ~50 fields per aircraft matter; filtering keeps the
    // document small enough to parse comfortably.
    JsonDocument filter;
    filter["total"] = true;
    JsonObject entry = filter["ac"].add<JsonObject>();
    entry["flight"] = true;
    entry["r"] = true;
    entry["t"] = true;
    entry["alt_baro"] = true;
    entry["gs"] = true;
    entry["track"] = true;
    entry["dst"] = true;
    entry["dir"] = true;
    entry["baro_rate"] = true;

    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
    http.end();

    if (error) {
        log_i("[traffic] parse failed: %s heap=%u",
              error.c_str(),
              static_cast<unsigned>(ESP.getFreeHeap()));
        return false;
    }

    // Counted here rather than taken from the API total, so the footer matches
    // what the page actually lists.
    uint16_t airborne = 0;

    for (JsonObject item : doc["ac"].as<JsonArray>()) {
        AircraftInfo info = {};
        copy_field(info.callsign, sizeof(info.callsign), item["flight"].as<const char*>());
        copy_field(info.registration, sizeof(info.registration), item["r"].as<const char*>(), true);
        copy_field(info.type, sizeof(info.type), item["t"].as<const char*>());

        // alt_baro is the string "ground" for aircraft on the surface.
        info.onGround = item["alt_baro"].is<const char*>();
        info.altitudeFt = info.onGround ? 0.0f : item["alt_baro"].as<float>();
        info.groundSpeedKt = item["gs"].as<float>();
        info.trackDeg = item["track"].as<float>();
        info.distanceNm = item["dst"].as<float>();
        info.bearingDeg = item["dir"].as<float>();
        info.verticalRateFpm = item["baro_rate"].as<float>();

        // Drop entries whose callsign was filler rather than a real identifier.
        if (strlen(info.callsign) < 3) {
            continue;
        }

        // Ground vehicles and parked aircraft sit closest to the station and
        // would otherwise take the featured slot on an air traffic page.
        if (info.onGround || info.groundSpeedKt < 40.0f || info.altitudeFt < 200.0f) {
            continue;
        }

        ++airborne;

        if (operator_code_from_callsign(info.callsign, info.operatorIcao)) {
            const AirlineEntry* airline = lookup_airline(info.operatorIcao);
            if (airline != nullptr) {
                info.airlineName = airline->name;
                info.airlineIata = airline->iata;
            }
        }

        insert_by_distance(staging, info);
    }

    // The featured slot is the nearest flight we can name, so the page shows an
    // airline and its logo whenever one is in range. The nearest aircraft
    // overall still appears, just in the list below. Falls back to plain
    // distance order when nothing in range is identifiable.
    for (uint8_t i = 1; i < staging.count; ++i) {
        if (staging.aircraft[i].airlineIata != nullptr) {
            if (staging.aircraft[0].airlineIata == nullptr) {
                const AircraftInfo featured = staging.aircraft[i];
                for (uint8_t j = i; j > 0; --j) {
                    staging.aircraft[j] = staging.aircraft[j - 1];
                }
                staging.aircraft[0] = featured;
            }
            break;
        }
    }

    staging.totalInRange = airborne;
    staging.isValid = true;
    data = staging;
    return true;
}

bool AircraftAPI::fetchAirlineLogo(const char* iata, AirlineLogo& logo) {
    if (iata == nullptr || strlen(iata) < 2) {
        return false;
    }

    if (cachedLogo.isValid && strncmp(cachedLogo.iata, iata, sizeof(cachedLogo.iata) - 1) == 0) {
        logo = cachedLogo;
        return true;
    }

    if (strncmp(missingIata, iata, sizeof(missingIata) - 1) == 0) {
        return false;
    }

    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    char url[96];
    snprintf(url, sizeof(url), "%s/%s.png", LOGO_HOST, iata);

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(10000);
    if (!http.begin(client, url)) {
        return false;
    }

    const int status = http.GET();
    if (status != HTTP_CODE_OK) {
        // A redirect or 404 means this airline has no logo on the CDN; remember
        // it so we do not ask again. Transport errors are not cached.
        http.end();
        if (status > 0) {
            snprintf(missingIata, sizeof(missingIata), "%s", iata);
        }
        return false;
    }

    uint8_t* encoded = static_cast<uint8_t*>(heap_caps_malloc(MAX_LOGO_BYTES, MALLOC_CAP_SPIRAM));
    if (encoded == nullptr) {
        encoded = static_cast<uint8_t*>(malloc(MAX_LOGO_BYTES));
    }
    if (encoded == nullptr) {
        http.end();
        return false;
    }

    // writeToStream() handles both identity and chunked encoding, unlike reading
    // getStream() directly against a Content-Length that the CDN may not send.
    MemoryStream sink(encoded, MAX_LOGO_BYTES);
    http.writeToStream(&sink);
    http.end();

    const size_t encodedSize = sink.size();
    if (encodedSize == 0) {
        free(encoded);
        return false;
    }

    // Hand the encoded PNG to LVGL and let its registered decoder do the work.
    unsigned width = 0;
    unsigned height = 0;
    // IHDR carries width at bytes 16-19 and height at 20-23, big endian.
    if (encodedSize > 24 && memcmp(encoded, "\x89PNG", 4) == 0) {
        width = (static_cast<unsigned>(encoded[16]) << 24) | (static_cast<unsigned>(encoded[17]) << 16) |
                (static_cast<unsigned>(encoded[18]) << 8) | encoded[19];
        height = (static_cast<unsigned>(encoded[20]) << 24) | (static_cast<unsigned>(encoded[21]) << 16) |
                 (static_cast<unsigned>(encoded[22]) << 8) | encoded[23];
    }

    if (width > 256 || height > 256) {
        width = 0;
        height = 0;
    }

    if (width == 0 || height == 0) {
        free(encoded);
        snprintf(missingIata, sizeof(missingIata), "%s", iata);
        return false;
    }

    if (cachedLogo.isValid && cachedLogo.png != nullptr) {
        free(cachedLogo.png);
    }

    cachedLogo.png = encoded;
    cachedLogo.pngSize = encodedSize;
    cachedLogo.width = static_cast<uint16_t>(width);
    cachedLogo.height = static_cast<uint16_t>(height);
    cachedLogo.isValid = true;
    snprintf(cachedLogo.iata, sizeof(cachedLogo.iata), "%s", iata);
    missingIata[0] = '\0';

    logo = cachedLogo;
    return true;
}

bool AircraftAPI::fetchRouteFromAdsbdb(const char* callsign, RouteInfo& route) {
    char url[96];
    snprintf(url, sizeof(url), "%s/v0/callsign/%s", ROUTE_HOST, callsign);

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(10000);
    if (!http.begin(client, url)) {
        return false;
    }

    const int status = http.GET();
    if (status != HTTP_CODE_OK) {
        http.end();
        return false;
    }

    JsonDocument filter;
    JsonObject flightroute = filter["response"]["flightroute"].to<JsonObject>();
    flightroute["origin"]["iata_code"] = true;
    flightroute["origin"]["municipality"] = true;
    flightroute["destination"]["iata_code"] = true;
    flightroute["destination"]["municipality"] = true;

    JsonDocument doc;
    const DeserializationError error =
        deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
    http.end();

    if (error) {
        return false;
    }

    JsonObject origin = doc["response"]["flightroute"]["origin"];
    JsonObject destination = doc["response"]["flightroute"]["destination"];
    if (origin.isNull() || destination.isNull()) {
        return false;
    }

    RouteInfo resolved = {};
    copy_text(resolved.fromCode, sizeof(resolved.fromCode), origin["iata_code"].as<const char*>());
    copy_text(resolved.toCode, sizeof(resolved.toCode), destination["iata_code"].as<const char*>());
    copy_text(resolved.fromCity, sizeof(resolved.fromCity), origin["municipality"].as<const char*>());
    copy_text(resolved.toCity, sizeof(resolved.toCity), destination["municipality"].as<const char*>());

    // Municipalities can carry a region ("Paisley, Renfrewshire"); the city
    // alone is what fits on one line.
    char* comma = strchr(resolved.fromCity, ',');
    if (comma != nullptr) {
        *comma = '\0';
    }
    comma = strchr(resolved.toCity, ',');
    if (comma != nullptr) {
        *comma = '\0';
    }
    snprintf(resolved.callsign, sizeof(resolved.callsign), "%s", callsign);

    if (resolved.fromCode[0] == '\0' || resolved.toCode[0] == '\0') {
        return false;
    }

    resolved.isValid = true;
    route = resolved;
    return true;
}

// hexdb answers with a plain "EGCC-EGLL" pair, or "n/a" when it has no route.
bool AircraftAPI::fetchRouteFromHexdb(const char* callsign, RouteInfo& route) {
    char url[128];
    snprintf(url, sizeof(url), "%s/callsign-route?callsign=%s", ROUTE_FALLBACK_HOST, callsign);

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(10000);
    if (!http.begin(client, url)) {
        return false;
    }

    const int status = http.GET();
    if (status != HTTP_CODE_OK) {
        http.end();
        return false;
    }

    String body = http.getString();
    http.end();
    body.trim();

    const int separator = body.indexOf('-');
    if (body.length() < 5 || separator <= 0 || body.startsWith("n/a")) {
        return false;
    }

    // Multi-leg routes list every stop; the first and last are what matter.
    char fromIcao[8];
    char toIcao[8];
    copy_field(fromIcao, sizeof(fromIcao), body.substring(0, separator).c_str());
    copy_field(toIcao, sizeof(toIcao), body.substring(body.lastIndexOf('-') + 1).c_str());
    if (strlen(fromIcao) < 3 || strlen(toIcao) < 3) {
        return false;
    }

    RouteInfo resolved = {};
    if (!lookupAirport(fromIcao, resolved.fromCode, sizeof(resolved.fromCode), resolved.fromCity, sizeof(resolved.fromCity))) {
        snprintf(resolved.fromCode, sizeof(resolved.fromCode), "%s", fromIcao);
    }
    if (!lookupAirport(toIcao, resolved.toCode, sizeof(resolved.toCode), resolved.toCity, sizeof(resolved.toCity))) {
        snprintf(resolved.toCode, sizeof(resolved.toCode), "%s", toIcao);
    }

    snprintf(resolved.callsign, sizeof(resolved.callsign), "%s", callsign);
    resolved.isValid = true;
    route = resolved;
    return true;
}

// Turns an ICAO airport code into the IATA code and name the page displays.
bool AircraftAPI::lookupAirport(const char* icao, char* code, size_t codeSize, char* city, size_t citySize) {
    char url[128];
    snprintf(url, sizeof(url), "%s/api/v1/airport/icao/%s", ROUTE_FALLBACK_HOST, icao);

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(10000);
    if (!http.begin(client, url)) {
        return false;
    }

    if (http.GET() != HTTP_CODE_OK) {
        http.end();
        return false;
    }

    JsonDocument filter;
    filter["iata"] = true;
    filter["airport"] = true;

    JsonDocument doc;
    const DeserializationError error =
        deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
    http.end();

    if (error) {
        return false;
    }

    copy_field(code, codeSize, doc["iata"].as<const char*>());
    copy_text(city, citySize, doc["airport"].as<const char*>());

    // "Manchester Airport" reads better as just "Manchester" next to the codes.
    char* suffix = strstr(city, " Airport");
    if (suffix != nullptr) {
        *suffix = '\0';
    }

    return strlen(code) >= 3;
}

bool AircraftAPI::fetchRoute(const char* callsign, RouteInfo& route) {
    if (callsign == nullptr || strlen(callsign) < 3) {
        return false;
    }

    if (cachedRoute.isValid && strcmp(cachedRoute.callsign, callsign) == 0) {
        route = cachedRoute;
        return true;
    }

    if (strcmp(missingCallsign, callsign) == 0) {
        return false;
    }

    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    RouteInfo resolved = {};
    if (!fetchRouteFromAdsbdb(callsign, resolved) && !fetchRouteFromHexdb(callsign, resolved)) {
        // Neither source knows it; do not ask again for this callsign.
        snprintf(missingCallsign, sizeof(missingCallsign), "%s", callsign);
        return false;
    }

    cachedRoute = resolved;
    missingCallsign[0] = '\0';
    route = resolved;
    return true;
}
