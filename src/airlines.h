#ifndef AIRLINES_H
#define AIRLINES_H

// Maps the 3-letter ICAO operator prefix of a callsign to an IATA code (used to
// build the logo URL) and a display name. Names stay ASCII: the built-in
// Montserrat fonts have no accented glyphs and render them as blank boxes.
struct AirlineEntry {
    const char* icao;
    const char* iata;
    const char* name;
};

// Unknown operators are reported as-is rather than guessed at, so this list only
// holds mappings that are certain.
static const AirlineEntry AIRLINES[] = {
    {"BAW", "BA", "British Airways"},
    {"SHT", "BA", "British Airways"},
    {"EZY", "U2", "easyJet"},
    {"EJU", "EC", "easyJet Europe"},
    {"EZS", "DS", "easyJet Switzerland"},
    {"RYR", "FR", "Ryanair"},
    {"RUK", "RK", "Ryanair UK"},
    {"EXS", "LS", "Jet2"},
    {"LOG", "LM", "Loganair"},
    {"TOM", "BY", "TUI Airways"},
    {"VIR", "VS", "Virgin Atlantic"},
    {"EIN", "EI", "Aer Lingus"},
    {"WZZ", "W6", "Wizz Air"},
    {"WUK", "W9", "Wizz Air UK"},
    {"DLH", "LH", "Lufthansa"},
    {"CLH", "CL", "Lufthansa CityLine"},
    {"GEC", "LH", "Lufthansa Cargo"},
    {"AFR", "AF", "Air France"},
    {"KLM", "KL", "KLM"},
    {"KLC", "WA", "KLM Cityhopper"},
    {"SAS", "SK", "SAS"},
    {"SWR", "LX", "Swiss"},
    {"AUA", "OS", "Austrian Airlines"},
    {"BEL", "SN", "Brussels Airlines"},
    {"TAP", "TP", "TAP Air Portugal"},
    {"IBE", "IB", "Iberia"},
    {"IBS", "I2", "Iberia Express"},
    {"VLG", "VY", "Vueling"},
    {"AEA", "UX", "Air Europa"},
    {"NAX", "DY", "Norwegian"},
    {"FIN", "AY", "Finnair"},
    {"ICE", "FI", "Icelandair"},
    {"LOT", "LO", "LOT Polish Airlines"},
    {"AEE", "A3", "Aegean Airlines"},
    {"THY", "TK", "Turkish Airlines"},
    {"PGT", "PC", "Pegasus Airlines"},
    {"BTI", "BT", "airBaltic"},
    {"LGL", "LG", "Luxair"},
    {"CFG", "DE", "Condor"},
    {"EWG", "EW", "Eurowings"},
    {"TFL", "OR", "TUI fly Netherlands"},
    {"SXS", "XQ", "SunExpress"},
    {"MSR", "MS", "EgyptAir"},
    {"RAM", "AT", "Royal Air Maroc"},
    {"DAH", "AH", "Air Algerie"},
    {"TAR", "TU", "Tunisair"},
    {"ELY", "LY", "El Al"},
    {"RJA", "RJ", "Royal Jordanian"},
    {"UAE", "EK", "Emirates"},
    {"ETD", "EY", "Etihad Airways"},
    {"QTR", "QR", "Qatar Airways"},
    {"SVA", "SV", "Saudia"},
    {"GFA", "GF", "Gulf Air"},
    {"KAC", "KU", "Kuwait Airways"},
    {"OMA", "WY", "Oman Air"},
    {"AIC", "AI", "Air India"},
    {"PIA", "PK", "Pakistan Intl"},
    {"SIA", "SQ", "Singapore Airlines"},
    {"MAS", "MH", "Malaysia Airlines"},
    {"THA", "TG", "Thai Airways"},
    {"CPA", "CX", "Cathay Pacific"},
    {"CCA", "CA", "Air China"},
    {"CES", "MU", "China Eastern"},
    {"CSN", "CZ", "China Southern"},
    {"JAL", "JL", "Japan Airlines"},
    {"ANA", "NH", "All Nippon Airways"},
    {"KAL", "KE", "Korean Air"},
    {"AAR", "OZ", "Asiana Airlines"},
    {"AAL", "AA", "American Airlines"},
    {"UAL", "UA", "United Airlines"},
    {"DAL", "DL", "Delta Air Lines"},
    {"ACA", "AC", "Air Canada"},
    {"TSC", "TS", "Air Transat"},
    {"WJA", "WS", "WestJet"},
    {"JBU", "B6", "JetBlue"},
    {"AMX", "AM", "Aeromexico"},
    {"AVA", "AV", "Avianca"},
    {"LAN", "LA", "LATAM"},
    {"ARG", "AR", "Aerolineas Argentinas"},
    {"QFA", "QF", "Qantas"},
    {"ANZ", "NZ", "Air New Zealand"},
    {"ETH", "ET", "Ethiopian Airlines"},
    {"KQA", "KQ", "Kenya Airways"},
    {"SAA", "SA", "South African Airways"},
    {"FDX", "FX", "FedEx"},
    {"UPS", "5X", "UPS Airlines"},
    {"CLX", "CV", "Cargolux"},
    {"BCS", "QY", "DHL Air"},
    {"TAY", "3V", "ASL Airlines"}
};

static constexpr size_t AIRLINE_COUNT = sizeof(AIRLINES) / sizeof(AIRLINES[0]);

#endif
