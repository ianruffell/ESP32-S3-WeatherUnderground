# ESP32-S3 Weather Display

Touch-friendly weather dashboard for an ESP32-S3 display panel using Arduino, LVGL, and PlatformIO.

![Device screenshot](screenshot.png)

## Features

- Instrument-cluster dashboard: black canvas, outlined state tags, hairline rules
- ProWeatherLive or Weather Underground PWS current conditions
- Swipeable weather pages for up to 5 locations
- Analog clock face alongside digital time, date, sunrise, sunset, and moon phase
- Temperature and pressure trend graphs, kept per location
- Live air traffic page: nearby aircraft with airline logos, route, altitude, speed, range and track
- Pages rotate automatically every 30 seconds (the setup page is skipped)
- Wind speed with compass dial and cardinal direction
- Wi-Fi and data status indicators in the header
- On-device setup portal for Wi-Fi and weather-provider settings
- QR code page for opening the setup portal from the local network
- Automatic weather retry backoff when API requests fail

## Hardware / Software

- Board target: `4d_systems_esp32s3_gen4_r8n16`
- Framework: Arduino
- UI: LVGL
- Build system: PlatformIO

## Project Layout

- `platformio.ini`: PlatformIO environment and dependencies
- `src/main.cpp`: app lifecycle, Wi-Fi, scheduling, page navigation
- `src/ui.cpp`: LVGL dashboard UI
- `src/weather.cpp`: Weather Underground fetch and parsing
- `src/web_ui.cpp`: local setup portal
- `src/config.h`: default timing and hardware constants

## Setup

1. Install PlatformIO.
2. Connect the ESP32-S3 board over USB.
3. Build the firmware:

```bash
~/.platformio/penv/bin/pio run
```

4. Flash the board:

```bash
~/.platformio/penv/bin/pio run -t upload --upload-port /dev/cu.usbserial-110
```

5. Open the device setup page:

- On first boot, the device can start a setup access point.
- After joining Wi-Fi, use the on-device QR page or the local IP shown in serial logs to open the setup portal.

## Configuration

Most user-facing configuration is handled through the setup web portal:

- Wi-Fi SSID and password
- Weather provider
- Weather Underground API key or ProWeatherLive access token
- Station IDs / location pages
- Air traffic radius, and whether the traffic page is shown

Static defaults and hardware mappings live in `src/config.h`.

### ProWeatherLive

ProWeatherLive does not publish a supported read API. This integration follows the
authenticated services used by its own dashboard, so a future service update may
require a firmware update.

1. Log in to `https://proweatherlive.net` in a desktop browser.
2. Open the browser developer console on that page and run:

```javascript
copy(JSON.parse(localStorage.getItem("login")).jwt)
```

3. Open the ESP32 setup page, choose **ProWeatherLive**, paste the copied value
   into **ProWeatherLive access token**, and enter the station's WSID as its
   Station ID.
4. Save and restart.

The token is a sensitive account credential. Enter it only on the local setup
page, do not commit it to this repository, and replace it if it is exposed. The
station key/WSPD is an upload credential and is not used by this firmware.

### Air Traffic

The traffic page shows aircraft around the first location page's coordinates,
using the free [adsb.lol](https://api.adsb.lol) feed. No account or API key is
needed. It is polled only while the page is on screen, roughly every 20 seconds.

The featured aircraft is the nearest flight whose operator can be identified, so
an airline and logo are shown whenever one is in range; the nearest aircraft
overall still appears in the list below it. Ground vehicles and parked aircraft
are filtered out. Aircraft whose operator is not in `src/airlines.h` show their
ICAO callsign prefix instead of a name, and no logo.

Departure and destination for the featured flight come from
[adsbdb.com](https://api.adsbdb.com), looked up by callsign and cached until the
featured aircraft changes. ADS-B itself carries no route information, so flights
the database does not know show "ROUTE UNKNOWN".

Airline logos are fetched on demand from `images.kiwi.com` and cached one at a
time in PSRAM. Not every airline has one there (United, for instance), in which
case the operator code is shown instead. This is an unofficial use of that CDN, and airline logos are
trademarks of their respective owners.

## Notes

- The default weather refresh interval is 10 minutes.
- Failed weather requests back off automatically to reduce API hammering.
- The repo ignores PlatformIO build output and editor-specific local files.
