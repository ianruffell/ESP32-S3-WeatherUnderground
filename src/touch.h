#ifndef TOUCH_H
#define TOUCH_H

#include <Arduino.h>

struct TouchPoint {
    bool touched = false;
    int16_t x = 0;
    int16_t y = 0;
};

class TouchController {
public:
    void begin();
    bool isAvailable() const;
    bool read(TouchPoint& point);

private:
    bool initialized = false;
    bool available = false;
    uint8_t address = 0;
    bool readRegister(uint16_t reg, uint8_t* data, size_t length);
    bool writeRegister(uint16_t reg, const uint8_t* data, size_t length);
};

#endif
