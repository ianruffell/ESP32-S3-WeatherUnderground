#ifndef WEB_UI_H
#define WEB_UI_H

#include <Arduino.h>
#include <IPAddress.h>
#include "settings.h"

class WebPortal {
public:
    WebPortal();

    void begin(const DeviceSettings& settings);
    void loop();
    void setSettings(const DeviceSettings& settings);
    void setNetworkState(bool apActive,
                         bool staConnected,
                         const String& apSsid,
                         const IPAddress& apIp,
                         const String& staSsid,
                         const IPAddress& staIp);
    bool takePendingSettings(DeviceSettings& settings);

private:
    class Impl;
    Impl* impl;
};

#endif
