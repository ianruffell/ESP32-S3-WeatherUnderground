#ifndef AIRCRAFT_H
#define AIRCRAFT_H

#include <Arduino.h>

static constexpr uint8_t MAX_TRACKED_AIRCRAFT = 5;

struct AircraftInfo {
    char callsign[12];
    char registration[12];
    char type[8];
    char operatorIcao[4];   // empty when the callsign is not an airline flight
    const char* airlineName; // nullptr when the operator is unknown
    const char* airlineIata; // nullptr when no logo can be looked up
    float distanceNm;
    float bearingDeg;       // from the station towards the aircraft
    float altitudeFt;
    float groundSpeedKt;
    float trackDeg;
    float verticalRateFpm;
    bool onGround;
};

struct TrafficData {
    AircraftInfo aircraft[MAX_TRACKED_AIRCRAFT];
    uint8_t count;        // entries populated in `aircraft`
    uint16_t totalInRange; // everything the API reported inside the radius
    uint16_t radiusNm;
    uint16_t ageSeconds;   // how long ago this data was actually fetched
    bool isValid;
};

// Holds the airline logo as the encoded PNG in PSRAM; LVGL's own PNG decoder
// turns it into pixels at draw time.
struct AirlineLogo {
    uint8_t* png;
    size_t pngSize;
    uint16_t width;
    uint16_t height;
    char iata[4];
    bool isValid;
};

// Departure and destination for a callsign. ADS-B does not carry this, so it is
// looked up separately and only for the featured aircraft.
struct RouteInfo {
    char fromCode[6];
    char toCode[6];
    char fromCity[24];
    char toCity[24];
    char callsign[12];
    bool isValid;
};

class AircraftAPI {
public:
    bool fetchTraffic(double latitude, double longitude, uint16_t radiusNm, TrafficData& data);

    // Resolves a callsign to its route, reusing the cached answer when the
    // callsign is unchanged. False when the route is unknown.
    bool fetchRoute(const char* callsign, RouteInfo& route);

    // Fetches and decodes `iata`'s logo, reusing the cached one when it matches.
    // Returns false when no logo exists for that airline.
    bool fetchAirlineLogo(const char* iata, AirlineLogo& logo);

private:
    AirlineLogo cachedLogo = {};
    char missingIata[4] = {0};     // last code known to have no logo, to avoid refetching
    RouteInfo cachedRoute = {};    // last resolved route, keyed by callsign
    char missingCallsign[12] = {0}; // last callsign with no route, to avoid refetching
};

#endif
