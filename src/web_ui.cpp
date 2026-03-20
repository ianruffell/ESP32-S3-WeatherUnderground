#include "web_ui.h"
#include <WebServer.h>

namespace {
String htmlEscape(const String& input) {
    String out;
    out.reserve(input.length() + 16);
    for (size_t i = 0; i < input.length(); ++i) {
        char c = input[i];
        switch (c) {
            case '&': out += F("&amp;"); break;
            case '<': out += F("&lt;"); break;
            case '>': out += F("&gt;"); break;
            case '"': out += F("&quot;"); break;
            default: out += c; break;
        }
    }
    return out;
}

String ipToString(const IPAddress& ip) {
    if (ip == IPAddress()) {
        return String("-");
    }
    return ip.toString();
}

String trimCopy(String value) {
    value.trim();
    return value;
}

String jsStringEscape(const String& input) {
    String out;
    out.reserve(input.length() + 16);
    for (size_t i = 0; i < input.length(); ++i) {
        char c = input[i];
        switch (c) {
            case '\\': out += F("\\\\"); break;
            case '\'': out += F("\\'"); break;
            case '\n': out += F("\\n"); break;
            case '\r': break;
            default: out += c; break;
        }
    }
    return out;
}
}

class WebPortal::Impl {
public:
    WebServer server{80};
    DeviceSettings currentSettings;
    DeviceSettings pendingSettings;
    bool started = false;
    bool hasPendingSettings = false;
    bool apActive = false;
    bool staConnected = false;
    String apSsid;
    IPAddress apIp;
    String staSsid;
    IPAddress staIp;

    void setupRoutes() {
        server.on("/", HTTP_GET, [this]() { handleRoot(); });
        server.on("/save", HTTP_POST, [this]() { handleSave(); });
        server.on("/api/settings", HTTP_GET, [this]() { handleApiSettings(); });
        server.onNotFound([this]() { server.sendHeader("Location", "/"); server.send(302, "text/plain", ""); });
    }

    void handleRoot() {
        server.send(200, "text/html", renderPage());
    }

    void handleApiSettings() {
        String json = "{";
        json += "\"deviceName\":\"" + currentSettings.deviceName + "\",";
        json += "\"apActive\":" + String(apActive ? "true" : "false") + ",";
        json += "\"staConnected\":" + String(staConnected ? "true" : "false") + ",";
        json += "\"apIp\":\"" + ipToString(apIp) + "\",";
        json += "\"staIp\":\"" + ipToString(staIp) + "\"";
        json += "}";
        server.send(200, "application/json", json);
    }

    void handleSave() {
        pendingSettings = currentSettings;
        pendingSettings.wifiSsid = trimCopy(server.arg("wifi_ssid"));
        pendingSettings.wifiPassword = server.arg("wifi_password");
        pendingSettings.weatherApiKey = trimCopy(server.arg("api_key"));

        int requestedPageCount = server.arg("page_count").toInt();
        if (requestedPageCount < 1) {
            requestedPageCount = 1;
        }
        if (requestedPageCount > MAX_WEATHER_PAGES) {
            requestedPageCount = MAX_WEATHER_PAGES;
        }

        pendingSettings.weatherPageCount = static_cast<uint8_t>(requestedPageCount);
        for (uint8_t i = 0; i < MAX_WEATHER_PAGES; ++i) {
            pendingSettings.weatherPages[i].name = "";
            pendingSettings.weatherPages[i].stationId = "";
            pendingSettings.weatherPages[i].timeZone = pendingSettings.timeZone;
            pendingSettings.weatherPages[i].latitude = pendingSettings.locationLat;
            pendingSettings.weatherPages[i].longitude = pendingSettings.locationLon;
        }

        for (int i = 0; i < requestedPageCount; ++i) {
            String nameKey = "page_name_";
            nameKey += i;
            String stationKey = "page_station_";
            stationKey += i;
            String timeZoneKey = "page_timezone_";
            timeZoneKey += i;
            String latKey = "page_latitude_";
            latKey += i;
            String lonKey = "page_longitude_";
            lonKey += i;
            pendingSettings.weatherPages[i].name = trimCopy(server.arg(nameKey));
            pendingSettings.weatherPages[i].stationId = trimCopy(server.arg(stationKey));
            pendingSettings.weatherPages[i].timeZone = trimCopy(server.arg(timeZoneKey));
            String latitudeArg = trimCopy(server.arg(latKey));
            String longitudeArg = trimCopy(server.arg(lonKey));
            pendingSettings.weatherPages[i].latitude = latitudeArg.isEmpty() ? pendingSettings.locationLat : latitudeArg.toFloat();
            pendingSettings.weatherPages[i].longitude = longitudeArg.isEmpty() ? pendingSettings.locationLon : longitudeArg.toFloat();
        }

        pendingSettings.sanitizeWeatherPages();

        hasPendingSettings = true;
        currentSettings = pendingSettings;

        String html;
        html.reserve(512);
        html += F("<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>");
        html += F("<meta http-equiv='refresh' content='4; url=/'><style>body{font-family:system-ui;background:#0b1620;color:#eef4f8;padding:24px}a{color:#8bd3ff}</style></head><body>");
        html += F("<h1>Settings Saved</h1><p>The device will restart and apply your changes.</p><p><a href='/'>Return to settings</a></p></body></html>");
        server.send(200, "text/html", html);
    }

    String renderPage() const {
        String html;
        html.reserve(16384);
        html += F("<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>");
        html += F("<title>Weather Display Setup</title><style>");
        html += F("body{margin:0;font-family:system-ui;background:#09131a;color:#ecf4f8}main{max-width:860px;margin:0 auto;padding:24px}");
        html += F(".hero,.card{background:linear-gradient(180deg,#102737,#17384d);border:1px solid #2b5168;border-radius:20px;padding:20px;box-shadow:0 20px 50px rgba(0,0,0,.25)}");
        html += F(".hero{margin-bottom:20px}.row{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:16px}.card{margin-bottom:16px}");
        html += F(".stack{display:grid;gap:16px}.location-card{background:#0d1d28;border:1px solid #2a5067;border-radius:18px;padding:16px}.location-head{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:14px}");
        html += F("label{display:block;font-size:13px;color:#a6c7da;margin-bottom:6px}input{width:100%;box-sizing:border-box;padding:12px 14px;border-radius:12px;border:1px solid #31556b;background:#08131b;color:#f5fbff}");
        html += F("button{border:0;border-radius:999px;padding:14px 22px;background:#69d2ff;color:#08202c;font-weight:700;cursor:pointer}button.secondary{background:#103041;color:#d8ebf6;border:1px solid #31556b}button.remove-location{padding:10px 16px;background:#203847;color:#dbeaf3}");
        html += F("button.remove-location:disabled{opacity:.45;cursor:not-allowed}h1,h2,h3{margin:0 0 12px}p{margin:0 0 10px;color:#bad0df}.pill{display:inline-block;padding:6px 10px;border-radius:999px;background:#0d1c25;border:1px solid #2e556b;margin:0 8px 8px 0;color:#d8ebf6}");
        html += F("</style></head><body><main>");
        html += F("<section class='hero'><h1>Weather Display Setup</h1>");
        html += F("<p>Use this page on first boot via the setup Wi-Fi, or later on your normal network to update the connection and add swipeable weather pages for other locations.</p>");
        html += F("<div class='pill'>STA: ");
        html += staConnected ? F("Connected") : F("Offline");
        html += F("</div><div class='pill'>STA IP: ");
        html += ipToString(staIp);
        html += F("</div><div class='pill'>AP: ");
        html += apActive ? F("Active") : F("Off");
        html += F("</div><div class='pill'>AP SSID: ");
        html += htmlEscape(apSsid.isEmpty() ? String("-") : apSsid);
        html += F("</div><div class='pill'>AP IP: ");
        html += ipToString(apIp);
        html += F("</div></section>");

        html += F("<form method='post' action='/save'>");
        html += F("<section class='card'><h2>Wi-Fi</h2><div class='row'>");
        html += field("Wi-Fi SSID", "wifi_ssid", currentSettings.wifiSsid, false);
        html += field("Wi-Fi password", "wifi_password", currentSettings.wifiPassword, true);
        html += F("</div></section>");

        html += F("<section class='card'><h2>Weather Service</h2><div class='row'>");
        html += field("API key", "api_key", currentSettings.weatherApiKey, true);
        html += F("</div></section>");

        html += F("<section class='card'><div class='location-head'><div><h2>Location Pages</h2><p>Add the stations you want to swipe between on the display. Timezone and coordinates are filled automatically after the station is fetched.</p></div>");
        html += F("<button type='button' class='secondary' id='add-location'>Add Location</button></div>");
        html += F("<input type='hidden' id='page_count' name='page_count' value='");
        html += String(currentSettings.weatherPageCount);
        html += F("'><div id='locations' class='stack'></div></section>");

        html += F("<button type='submit'>Save And Restart</button></form>");
        html += F("<script>");
        html += F("const maxLocations=");
        html += String(MAX_WEATHER_PAGES);
        html += F(";const locations=document.getElementById('locations');const countInput=document.getElementById('page_count');");
        html += F("function syncLocations(){const cards=[...locations.querySelectorAll('.location-card')];countInput.value=cards.length;cards.forEach((card,index)=>{card.querySelector('.location-title').textContent='Page '+(index+1);card.querySelectorAll('input[data-field]').forEach((input)=>{const field=input.dataset.field;input.name='page_'+field+'_'+index;input.id='page_'+field+'_'+index;});card.querySelectorAll('label[data-for]').forEach((label)=>{label.htmlFor='page_'+label.dataset.for+'_'+index;});const remove=card.querySelector('.remove-location');remove.disabled=cards.length===1;});document.getElementById('add-location').disabled=cards.length>=maxLocations;}");
        html += F("function addLocation(name='',station='',timezone='',latitude='',longitude=''){if(locations.children.length>=maxLocations)return;const card=document.createElement('div');card.className='location-card';card.innerHTML=\"<div class='location-head'><h3 class='location-title'>Page</h3><button type='button' class='remove-location'>Remove</button></div><div class='row'><div><label data-for='name'>Page name</label><input data-field='name' type='text'></div><div><label data-for='station'>Station ID</label><input data-field='station' type='text'></div><div><label data-for='timezone'>Timezone</label><input data-field='timezone' type='text' readonly></div><div><label data-for='latitude'>Latitude</label><input data-field='latitude' type='number' step='0.0001' readonly></div><div><label data-for='longitude'>Longitude</label><input data-field='longitude' type='number' step='0.0001' readonly></div></div>\";const inputs=card.querySelectorAll('input[data-field]');inputs[0].value=name;inputs[1].value=station;inputs[2].value=timezone;inputs[3].value=latitude;inputs[4].value=longitude;locations.appendChild(card);syncLocations();}");
        html += F("document.getElementById('add-location').addEventListener('click',()=>addLocation());locations.addEventListener('click',(event)=>{if(event.target.classList.contains('remove-location')){event.target.closest('.location-card').remove();if(!locations.children.length){addLocation();}syncLocations();}});");
        for (uint8_t i = 0; i < currentSettings.weatherPageCount && i < MAX_WEATHER_PAGES; ++i) {
            html += F("addLocation('");
            html += jsStringEscape(currentSettings.weatherPages[i].name);
            html += F("','");
            html += jsStringEscape(currentSettings.weatherPages[i].stationId);
            html += F("','");
            html += jsStringEscape(currentSettings.weatherPages[i].timeZone);
            html += F("','");
            html += String(currentSettings.weatherPages[i].latitude, 4);
            html += F("','");
            html += String(currentSettings.weatherPages[i].longitude, 4);
            html += F("');");
        }
        html += F("if(!locations.children.length){addLocation();}syncLocations();");
        html += F("</script></main></body></html>");
        return html;
    }

    String field(const char* label, const char* name, const String& value, bool password) const {
        String html;
        html.reserve(256);
        html += F("<div><label for='");
        html += name;
        html += F("'>");
        html += label;
        html += F("</label><input id='");
        html += name;
        html += F("' name='");
        html += name;
        html += F("' value='");
        html += htmlEscape(value);
        html += F("' ");
        html += password ? F("type='password'") : F("type='text'");
        html += F("></div>");
        return html;
    }
};

WebPortal::WebPortal() : impl(new Impl()) {
}

void WebPortal::begin(const DeviceSettings& settings) {
    impl->currentSettings = settings;
    if (!impl->started) {
        impl->setupRoutes();
        impl->server.begin();
        impl->started = true;
    }
}

void WebPortal::loop() {
    if (impl->started) {
        impl->server.handleClient();
    }
}

void WebPortal::setSettings(const DeviceSettings& settings) {
    impl->currentSettings = settings;
}

void WebPortal::setNetworkState(bool apActive,
                                bool staConnected,
                                const String& apSsid,
                                const IPAddress& apIp,
                                const String& staSsid,
                                const IPAddress& staIp) {
    impl->apActive = apActive;
    impl->staConnected = staConnected;
    impl->apSsid = apSsid;
    impl->apIp = apIp;
    impl->staSsid = staSsid;
    impl->staIp = staIp;
}

bool WebPortal::takePendingSettings(DeviceSettings& settings) {
    if (!impl->hasPendingSettings) {
        return false;
    }
    settings = impl->pendingSettings;
    impl->hasPendingSettings = false;
    return true;
}
