#include <Arduino.h>
#include <WiFi.h>
#include <Arduino_GFX_Library.h>
#include <math.h>
#include <time.h>
#include "config.h"
#include "settings.h"
#include "touch.h"
#include "weather.h"
#include "web_ui.h"
#include "ui.h"

Arduino_ESP32RGBPanel* displayBus = nullptr;
Arduino_ST7701_RGBPanel* panel = nullptr;
Arduino_GFX* gfx = nullptr;
lv_disp_t* disp;
lv_indev_t* indev;

WeatherAPI* weatherApi = nullptr;
TouchController touchController;
WeatherData weatherPages[MAX_WEATHER_PAGES];
DeviceSettings deviceSettings;
SettingsStore settingsStore;
WebPortal webPortal;

unsigned long lastUpdate = 0;
unsigned long lastClockUpdate = 0;
unsigned long lastReconnectAttempt = 0;
unsigned long rebootAt = 0;
unsigned long weatherRefreshIntervalMs = WEATHER_UPDATE_INTERVAL;
int lastAstronomyDay = -1;
bool setupApActive = false;
bool portalStarted = false;
bool rebootScheduled = false;
bool setupRequired = false;
bool swipeTracking = false;
bool swipeConsumed = false;
bool showingPortalQrPage = false;
bool lastPortalQrPageVisible = false;
uint8_t consecutiveWeatherFailures = 0;
int16_t swipeStartX = 0;
int16_t swipeStartY = 0;
uint8_t activeWeatherPageIndex = 0;
String setupApSsid;

static bool hasCompletedSetup(const DeviceSettings& settings) {
    return settings.hasWiFiCredentials() && settings.hasWeatherCredentials();
}

static uint8_t configuredWeatherPageCount() {
    return deviceSettings.configuredWeatherPageCount();
}

static uint8_t primaryDisplayPageCount() {
    const uint8_t weatherPageCount = configuredWeatherPageCount();
    return weatherPageCount == 0 ? 1 : weatherPageCount;
}

static const WeatherPageSettings& activeWeatherPageSettings() {
    if (configuredWeatherPageCount() == 0 || activeWeatherPageIndex >= MAX_WEATHER_PAGES) {
        return deviceSettings.weatherPages[0];
    }
    return deviceSettings.weatherPages[activeWeatherPageIndex];
}

static bool portalQrPageAvailable() {
    return !setupApActive && portalStarted && WiFi.status() == WL_CONNECTED;
}

static uint8_t totalDisplayPageCount() {
    return primaryDisplayPageCount() + (portalQrPageAvailable() ? 1 : 0);
}

static uint8_t portalQrPageIndex() {
    return primaryDisplayPageCount();
}

static String currentSetupPageUrl() {
    String url = "http://";
    url += WiFi.localIP().toString();
    return url;
}

static unsigned long nextWeatherRefreshIntervalMs(uint8_t consecutiveFailures) {
    unsigned long interval = WEATHER_UPDATE_INTERVAL;
    for (uint8_t i = 0; i < consecutiveFailures; ++i) {
        if (interval >= WEATHER_MAX_RETRY_INTERVAL / 2) {
            return WEATHER_MAX_RETRY_INTERVAL;
        }
        interval *= 2;
    }
    return interval;
}

static String formatUtcOffset(int minutesEastOfUtc) {
    char buffer[16];
    const char sign = minutesEastOfUtc < 0 ? '-' : '+';
    const int absoluteMinutes = abs(minutesEastOfUtc);
    const int hours = absoluteMinutes / 60;
    const int minutes = absoluteMinutes % 60;
    snprintf(buffer, sizeof(buffer), "UTC%c%02d:%02d", sign, hours, minutes);
    return String(buffer);
}

static int activeWeatherPageUtcOffsetMinutes() {
    const WeatherData& activeData = weatherPages[activeWeatherPageIndex];
    if (activeData.isValid) {
        return activeData.utcOffsetMinutes;
    }

    const String tz = activeWeatherPageSettings().timeZone;
    if (tz.startsWith("UTC") && tz.length() >= 9) {
        const int sign = tz.charAt(3) == '-' ? -1 : 1;
        const int hours = tz.substring(4, 6).toInt();
        const int minutes = tz.substring(7, 9).toInt();
        return sign * (hours * 60 + minutes);
    }

    return 0;
}

static bool activePageLocalTime(struct tm& out) {
    time_t now = time(nullptr);
    if (now < 100000) {
        return false;
    }

    now += static_cast<time_t>(activeWeatherPageUtcOffsetMinutes()) * 60;
    return gmtime_r(&now, &out) != nullptr;
}

static void clampActiveWeatherPageIndex() {
    const uint8_t count = configuredWeatherPageCount();
    if (count == 0) {
        activeWeatherPageIndex = 0;
    } else if (activeWeatherPageIndex >= count) {
        activeWeatherPageIndex = 0;
    }
}

static void showActiveWeatherPage(bool showLoadingWhenEmpty = true) {
    const uint8_t count = configuredWeatherPageCount();
    clampActiveWeatherPageIndex();

    if (count == 0) {
        ui_update_page("Add a location", 0, totalDisplayPageCount());
        if (showLoadingWhenEmpty) {
            ui_show_loading();
        }
        return;
    }

    const String title = deviceSettings.weatherPageTitle(activeWeatherPageIndex);
    ui_update_page(title.c_str(), activeWeatherPageIndex, totalDisplayPageCount());

    if (weatherPages[activeWeatherPageIndex].isValid) {
        ui_update_weather(weatherPages[activeWeatherPageIndex]);
    } else if (showLoadingWhenEmpty) {
        ui_show_loading();
    }
}

static void showPortalQrPage() {
    if (!portalQrPageAvailable()) {
        return;
    }

    String url = currentSetupPageUrl();
    String ssid = WiFi.SSID();
    showingPortalQrPage = true;
    ui_update_page("Setup Page", portalQrPageIndex(), totalDisplayPageCount());
    ui_show_portal_page(ssid.c_str(), url.c_str());
}

static double degToRad(double degrees) {
    return degrees * PI / 180.0;
}

static double radToDeg(double radians) {
    return radians * 180.0 / PI;
}

static double normalizeDegrees(double value) {
    while (value < 0.0) {
        value += 360.0;
    }
    while (value >= 360.0) {
        value -= 360.0;
    }
    return value;
}

static double normalizeHours(double value) {
    while (value < 0.0) {
        value += 24.0;
    }
    while (value >= 24.0) {
        value -= 24.0;
    }
    return value;
}

static int localUtcOffsetMinutes(const struct tm& localTime) {
    char offsetBuffer[8];
    if (strftime(offsetBuffer, sizeof(offsetBuffer), "%z", &localTime) != 5) {
        return 0;
    }

    int sign = (offsetBuffer[0] == '-') ? -1 : 1;
    int hours = (offsetBuffer[1] - '0') * 10 + (offsetBuffer[2] - '0');
    int minutes = (offsetBuffer[3] - '0') * 10 + (offsetBuffer[4] - '0');
    return sign * (hours * 60 + minutes);
}

static bool calculateSunEvent(const struct tm& localTime, bool sunrise, char* out, size_t outSize) {
    int dayOfYear = localTime.tm_yday + 1;
    const WeatherPageSettings& page = activeWeatherPageSettings();
    double longitudeHour = page.longitude / 15.0;
    double approxTime = dayOfYear + ((sunrise ? 6.0 : 18.0) - longitudeHour) / 24.0;

    double meanAnomaly = (0.9856 * approxTime) - 3.289;
    double trueLongitude = meanAnomaly + (1.916 * sin(degToRad(meanAnomaly))) + (0.020 * sin(2 * degToRad(meanAnomaly))) + 282.634;
    trueLongitude = normalizeDegrees(trueLongitude);

    double rightAscension = radToDeg(atan(0.91764 * tan(degToRad(trueLongitude))));
    rightAscension = normalizeDegrees(rightAscension);

    double longitudeQuadrant = floor(trueLongitude / 90.0) * 90.0;
    double rightAscensionQuadrant = floor(rightAscension / 90.0) * 90.0;
    rightAscension += longitudeQuadrant - rightAscensionQuadrant;
    rightAscension /= 15.0;

    double sinDeclination = 0.39782 * sin(degToRad(trueLongitude));
    double cosDeclination = cos(asin(sinDeclination));
    double cosHourAngle = (cos(degToRad(90.833)) - (sinDeclination * sin(degToRad(page.latitude)))) /
                          (cosDeclination * cos(degToRad(page.latitude)));

    if (cosHourAngle > 1.0 || cosHourAngle < -1.0) {
        snprintf(out, outSize, "--:--");
        return false;
    }

    double hourAngle = sunrise ? 360.0 - radToDeg(acos(cosHourAngle)) : radToDeg(acos(cosHourAngle));
    hourAngle /= 15.0;

    double localMeanTime = hourAngle + rightAscension - (0.06571 * approxTime) - 6.622;
    double utcHours = normalizeHours(localMeanTime - longitudeHour);
    double localHours = normalizeHours(utcHours + (activeWeatherPageUtcOffsetMinutes() / 60.0));

    int hours = static_cast<int>(localHours);
    int minutes = static_cast<int>((localHours - hours) * 60.0 + 0.5);
    if (minutes >= 60) {
        minutes -= 60;
        hours = (hours + 1) % 24;
    }

    snprintf(out, outSize, "%02d:%02d", hours, minutes);
    return true;
}

static float calculateMoonPhase() {
    time_t now = time(nullptr);
    double julianDate = 2440587.5 + (static_cast<double>(now) / 86400.0);
    double phase = fmod((julianDate - 2451550.1) / 29.53058867, 1.0);
    if (phase < 0.0) {
        phase += 1.0;
    }
    return static_cast<float>(phase);
}

static void updateWebPortalState() {
    if (!portalStarted) {
        return;
    }

    webPortal.setSettings(deviceSettings);
    webPortal.setNetworkState(
        setupApActive,
        WiFi.status() == WL_CONNECTED,
        setupApSsid,
        WiFi.softAPIP(),
        WiFi.SSID(),
        WiFi.localIP()
    );
}

static void beginWebPortalIfNeeded() {
    if (!portalStarted) {
        webPortal.begin(deviceSettings);
        portalStarted = true;
    } else {
        webPortal.setSettings(deviceSettings);
    }

    updateWebPortalState();
}

static void updateAstronomyIfNeeded(bool force = false) {
    struct tm localTime;
    if (!activePageLocalTime(localTime)) {
        return;
    }

    if (!force && localTime.tm_yday == lastAstronomyDay) {
        return;
    }

    char sunrise[8];
    char sunset[8];
    float moonPhase = calculateMoonPhase();
    calculateSunEvent(localTime, true, sunrise, sizeof(sunrise));
    calculateSunEvent(localTime, false, sunset, sizeof(sunset));
    ui_update_astronomy(sunrise, sunset, moonPhase);

    lastAstronomyDay = localTime.tm_yday;
}

static void applySettingsToRuntime() {
    deviceSettings.sanitizeWeatherPages();
    clampActiveWeatherPageIndex();

    if (weatherApi == nullptr) {
        weatherApi = new WeatherAPI("", deviceSettings.weatherApiKey.c_str());
    } else {
        weatherApi->setCredentials("", deviceSettings.weatherApiKey);
    }

    for (uint8_t i = 0; i < MAX_WEATHER_PAGES; ++i) {
        weatherPages[i] = {};
    }
}

static void scheduleRestart() {
    rebootScheduled = true;
    rebootAt = millis() + 1500;
}

static String setupAccessPointName() {
    String base = deviceSettings.deviceName;
    base.trim();
    if (base.isEmpty()) {
        base = DEFAULT_DEVICE_NAME;
    }
    base += SETUP_AP_SUFFIX;
    return base;
}

static void startSetupAccessPoint() {
    if (setupApActive) {
        return;
    }

    setupApSsid = setupAccessPointName();
    WiFi.mode(setupRequired ? WIFI_AP : WIFI_AP_STA);
    WiFi.softAP(setupApSsid.c_str());
    setupApActive = true;

    Serial.println("Setup access point active");
    Serial.print("SSID: ");
    Serial.println(setupApSsid);
    Serial.print("Setup URL: http://");
    Serial.println(WiFi.softAPIP());

    beginWebPortalIfNeeded();
    updateWebPortalState();

    String setupUrl = "http://";
    setupUrl += WiFi.softAPIP().toString();
    ui_show_setup(setupApSsid.c_str(), "", setupUrl.c_str());
}

static void stopSetupAccessPoint() {
    if (!setupApActive) {
        return;
    }

    WiFi.softAPdisconnect(true);
    setupApActive = false;
    setupApSsid = "";
    ui_hide_setup();
    updateWebPortalState();
}

static bool connectWiFi() {
    if (!deviceSettings.hasWiFiCredentials()) {
        Serial.println("No WiFi credentials saved.");
        return false;
    }

    Serial.print("Connecting to WiFi SSID: ");
    Serial.println(deviceSettings.wifiSsid);

    WiFi.disconnect(true, true);
    WiFi.mode(setupApActive ? WIFI_AP_STA : WIFI_STA);
    WiFi.setHostname(deviceSettings.deviceName.c_str());
    WiFi.setSleep(false);
    WiFi.begin(deviceSettings.wifiSsid.c_str(), deviceSettings.wifiPassword.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
        delay(250);
        Serial.print(".");
        if (portalStarted) {
            webPortal.loop();
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected.");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        return true;
    }

    Serial.println("\nWiFi connection failed.");
    return false;
}

static void syncClock() {
    configTime(0, 0, deviceSettings.ntpServer1.c_str(), deviceSettings.ntpServer2.c_str());
}

static void handlePendingSettingsSave() {
    DeviceSettings updatedSettings;
    if (!webPortal.takePendingSettings(updatedSettings)) {
        return;
    }

    if (updatedSettings.deviceName.isEmpty()) {
        updatedSettings.deviceName = DEFAULT_DEVICE_NAME;
    }

    deviceSettings = updatedSettings;
    deviceSettings.sanitizeWeatherPages();
    settingsStore.save(deviceSettings);
    setupRequired = !hasCompletedSetup(deviceSettings);
    activeWeatherPageIndex = 0;
    applySettingsToRuntime();
    Serial.println("Settings updated via web UI. Restart scheduled.");
    scheduleRestart();
}

static bool fetchWeatherPages() {
    const uint8_t pageCount = configuredWeatherPageCount();
    if (pageCount == 0) {
        return false;
    }

    Serial.println("Fetching weather data...");
    bool metadataChanged = false;
    bool anySuccess = false;

    for (uint8_t i = 0; i < pageCount; ++i) {
        const String title = deviceSettings.weatherPageTitle(i);
        Serial.print("Updating page: ");
        Serial.println(title);

        const float previousPressure = weatherPages[i].pressure;
        const bool hadPreviousPressure = weatherPages[i].isValid;

        WeatherData fetched = weatherPages[i];
        weatherApi->setCredentials(deviceSettings.weatherPages[i].stationId, deviceSettings.weatherApiKey);

        if (!weatherApi->fetchWeatherData(fetched)) {
            Serial.println("Failed to fetch weather data for page");
            if (i == activeWeatherPageIndex && !weatherPages[i].isValid) {
                ui_show_error("Connection error");
            }
            continue;
        }

        if (hadPreviousPressure) {
            const float pressureDelta = fetched.pressure - previousPressure;
            if (pressureDelta > 0.3f) {
                fetched.pressureTrend = 1;
            } else if (pressureDelta < -0.3f) {
                fetched.pressureTrend = -1;
            } else {
                fetched.pressureTrend = 0;
            }
        } else {
            fetched.pressureTrend = 0;
        }

        weatherPages[i] = fetched;
        anySuccess = true;

        WeatherPageSettings& pageSettings = deviceSettings.weatherPages[i];
        const String derivedTimeZone = formatUtcOffset(fetched.utcOffsetMinutes);
        if (fabs(pageSettings.latitude - fetched.latitude) > 0.001f ||
            fabs(pageSettings.longitude - fetched.longitude) > 0.001f ||
            pageSettings.timeZone != derivedTimeZone) {
            pageSettings.latitude = fetched.latitude;
            pageSettings.longitude = fetched.longitude;
            pageSettings.timeZone = derivedTimeZone;
            metadataChanged = true;
        }
    }

    if (metadataChanged) {
        settingsStore.save(deviceSettings);
    }

    showActiveWeatherPage(false);
    return anySuccess;
}

static void switchDisplayPage(int direction) {
    const uint8_t weatherPageCount = configuredWeatherPageCount();
    const uint8_t pageCount = totalDisplayPageCount();
    if (pageCount < 2) {
        return;
    }

    const int currentIndex = showingPortalQrPage
        ? static_cast<int>(portalQrPageIndex())
        : (weatherPageCount == 0 ? 0 : static_cast<int>(activeWeatherPageIndex));
    int nextIndex = currentIndex + direction;
    if (nextIndex < 0) {
        nextIndex = pageCount - 1;
    } else if (nextIndex >= pageCount) {
        nextIndex = 0;
    }

    if (portalQrPageAvailable() && nextIndex == portalQrPageIndex()) {
        showPortalQrPage();
        return;
    }

    showingPortalQrPage = false;
    ui_hide_portal_page();
    if (weatherPageCount > 0) {
        activeWeatherPageIndex = static_cast<uint8_t>(nextIndex);
    }
    showActiveWeatherPage(true);
    updateAstronomyIfNeeded(true);
}

static void handleTouchSwipe() {
    if (setupApActive || totalDisplayPageCount() < 2) {
        swipeTracking = false;
        swipeConsumed = false;
        return;
    }

    TouchPoint point;
    if (!touchController.read(point) || !touchController.isAvailable()) {
        return;
    }

    if (!point.touched) {
        swipeTracking = false;
        swipeConsumed = false;
        return;
    }

    if (!swipeTracking) {
        swipeTracking = true;
        swipeConsumed = false;
        swipeStartX = point.x;
        swipeStartY = point.y;
        return;
    }

    if (swipeConsumed) {
        return;
    }

    const int dx = point.x - swipeStartX;
    const int dy = point.y - swipeStartY;
    if (abs(dx) < 70 || abs(dx) < abs(dy) + 20) {
        return;
    }

    switchDisplayPage(dx < 0 ? 1 : -1);
    swipeConsumed = true;
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("Starting ESP32-S3 Weather Display...");

    settingsStore.begin();
    bool hasSavedSettings = settingsStore.load(deviceSettings);
    setupRequired = !hasSavedSettings || !hasCompletedSetup(deviceSettings);
    applySettingsToRuntime();

    ui_init();
    ui_show_loading();
    touchController.begin();

    bool wifiConnected = false;
    if (setupRequired) {
        WiFi.persistent(false);
        WiFi.setAutoConnect(false);
        WiFi.setAutoReconnect(false);
        WiFi.disconnect(true, true);
        WiFi.mode(WIFI_OFF);
        delay(100);
    } else {
        wifiConnected = connectWiFi();
    }

    if (setupRequired) {
        Serial.println("Setup incomplete. Starting setup mode.");
    }

    if (setupRequired || !wifiConnected) {
        startSetupAccessPoint();
    }

    beginWebPortalIfNeeded();
    updateWebPortalState();

    if (!setupRequired && WiFi.status() == WL_CONNECTED) {
        syncClock();
        stopSetupAccessPoint();
        updateAstronomyIfNeeded(true);
        if (portalStarted) {
            Serial.print("Web UI: http://");
            Serial.println(WiFi.localIP());
        }
    }

    if (setupApActive) {
        String setupUrl = "http://";
        setupUrl += WiFi.softAPIP().toString();
        ui_show_setup(setupApSsid.c_str(), "", setupUrl.c_str());
    }

    showingPortalQrPage = false;
    ui_hide_portal_page();
    showActiveWeatherPage(true);
    lastPortalQrPageVisible = portalQrPageAvailable();
    weatherRefreshIntervalMs = WEATHER_UPDATE_INTERVAL;
    consecutiveWeatherFailures = 0;

    lastUpdate = millis() - WEATHER_UPDATE_INTERVAL;
    lastReconnectAttempt = millis();
}

void loop() {
    webPortal.loop();
    handlePendingSettingsSave();

    if (rebootScheduled && millis() >= rebootAt) {
        ESP.restart();
    }

    lv_timer_handler();
    handleTouchSwipe();

    const bool portalPageAvailable = portalQrPageAvailable();
    if (portalPageAvailable != lastPortalQrPageVisible) {
        lastPortalQrPageVisible = portalPageAvailable;
        if (showingPortalQrPage && !portalPageAvailable) {
            showingPortalQrPage = false;
            ui_hide_portal_page();
            showActiveWeatherPage(true);
        } else if (!showingPortalQrPage) {
            showActiveWeatherPage(false);
        }
    }

    if (millis() - lastClockUpdate >= 1000) {
        const bool wifiConnected = WiFi.status() == WL_CONNECTED;
        bool canRefreshWeather = wifiConnected && deviceSettings.hasWeatherCredentials();
        unsigned long refreshSecondsRemaining = 0;
        char timeBuffer[10];
        char dateBuffer[20];
        const char* timeText = "--:--:--";
        const char* dateText = "-- --- ----";
        if (canRefreshWeather) {
            unsigned long elapsed = millis() - lastUpdate;
            unsigned long remaining = (elapsed >= weatherRefreshIntervalMs) ? 0 : (weatherRefreshIntervalMs - elapsed);
            refreshSecondsRemaining = (remaining + 999) / 1000;
        }

        struct tm localTime;
        if (activePageLocalTime(localTime)) {
            strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &localTime);
            strftime(dateBuffer, sizeof(dateBuffer), "%d %b %Y", &localTime);
            timeText = timeBuffer;
            dateText = dateBuffer;
        }

        ui_update_wifi_status(wifiConnected);
        ui_update_clock(timeText, dateText, refreshSecondsRemaining, canRefreshWeather);
        if (showingPortalQrPage && portalPageAvailable) {
            showPortalQrPage();
        }
        updateAstronomyIfNeeded();
        lastClockUpdate = millis();
    }

    if (WiFi.status() == WL_CONNECTED && deviceSettings.hasWeatherCredentials()) {
        if (millis() - lastUpdate >= weatherRefreshIntervalMs) {
            if (fetchWeatherPages()) {
                consecutiveWeatherFailures = 0;
                weatherRefreshIntervalMs = WEATHER_UPDATE_INTERVAL;
            } else {
                if (consecutiveWeatherFailures < 10) {
                    ++consecutiveWeatherFailures;
                }
                weatherRefreshIntervalMs = nextWeatherRefreshIntervalMs(consecutiveWeatherFailures);
                Serial.print("Weather fetch backoff (ms): ");
                Serial.println(weatherRefreshIntervalMs);
            }
            lastUpdate = millis();
        }
    } else if (!setupRequired && WiFi.status() != WL_CONNECTED && millis() - lastReconnectAttempt >= WIFI_RETRY_INTERVAL_MS) {
        lastReconnectAttempt = millis();
        if (connectWiFi()) {
            syncClock();
            updateAstronomyIfNeeded(true);
            stopSetupAccessPoint();
            lastPortalQrPageVisible = portalQrPageAvailable();
            if (showingPortalQrPage && lastPortalQrPageVisible) {
                showPortalQrPage();
            }
        } else {
            startSetupAccessPoint();
            lastPortalQrPageVisible = portalQrPageAvailable();
        }
        updateWebPortalState();
    }

    delay(50);
}
