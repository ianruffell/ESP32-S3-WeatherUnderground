# ESP32-S3 Weather Underground Display

Touch-friendly weather dashboard for an ESP32-S3 display panel using Arduino, LVGL, and PlatformIO.

## Features

- Weather Underground PWS current conditions
- Swipeable weather pages for up to 5 locations
- Clock, date, sunrise, sunset, and moon phase
- Wind speed card with compass-style direction needle
- Wi-Fi status indicator on the time panel
- On-device setup portal for Wi-Fi and Weather Underground settings
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
- Weather Underground API key
- Station IDs / location pages

Static defaults and hardware mappings live in `src/config.h`.

## Notes

- The default weather refresh interval is 10 minutes.
- Failed weather requests back off automatically to reduce API hammering.
- The repo ignores PlatformIO build output and editor-specific local files.
