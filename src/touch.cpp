#include "touch.h"
#include <Wire.h>
#include "config.h"

namespace {
constexpr uint16_t GT911_PRODUCT_ID_REG = 0x8140;
constexpr uint16_t GT911_STATUS_REG = 0x814E;
constexpr uint16_t GT911_FIRST_POINT_REG = 0x814F;
}

void TouchController::begin() {
    if (initialized) {
        return;
    }

    initialized = true;
    Wire.begin(TOUCH_SDA, TOUCH_SCL, 400000U);

    const uint8_t candidates[] = {TOUCH_GT911_PRIMARY_ADDR, TOUCH_GT911_FALLBACK_ADDR};
    uint8_t productId[4] = {};

    for (uint8_t candidate : candidates) {
        address = candidate;
        if (!readRegister(GT911_PRODUCT_ID_REG, productId, sizeof(productId))) {
            continue;
        }

        const bool looksLikeGt911 =
            productId[0] == '9' &&
            productId[1] == '1' &&
            productId[2] == '1';

        if (looksLikeGt911) {
            available = true;
            Serial.print("Touch controller detected at 0x");
            Serial.println(address, HEX);
            Serial.print("Touch product ID: ");
            Serial.write(productId, 3);
            Serial.println();
            return;
        }
    }

    address = 0;
    Serial.println("Touch controller not detected.");
}

bool TouchController::isAvailable() const {
    return available;
}

bool TouchController::read(TouchPoint& point) {
    point = {};

    if (!initialized) {
        begin();
    }
    if (!available) {
        return false;
    }

    uint8_t status = 0;
    if (!readRegister(GT911_STATUS_REG, &status, 1)) {
        return false;
    }

    if ((status & 0x80U) == 0) {
        return true;
    }

    const uint8_t touchCount = status & 0x0FU;
    if (touchCount == 0) {
        const uint8_t clear = 0;
        writeRegister(GT911_STATUS_REG, &clear, 1);
        return true;
    }

    uint8_t rawPoint[7] = {};
    if (!readRegister(GT911_FIRST_POINT_REG, rawPoint, sizeof(rawPoint))) {
        return false;
    }

    int16_t x = static_cast<int16_t>(rawPoint[1] | (rawPoint[2] << 8));
    int16_t y = static_cast<int16_t>(rawPoint[3] | (rawPoint[4] << 8));

#if TOUCH_SWAP_XY
    const int16_t temp = x;
    x = y;
    y = temp;
#endif
#if TOUCH_MIRROR_X
    x = DISPLAY_WIDTH - 1 - x;
#endif
#if TOUCH_MIRROR_Y
    y = DISPLAY_HEIGHT - 1 - y;
#endif

    if (x < 0) {
        x = 0;
    } else if (x >= DISPLAY_WIDTH) {
        x = DISPLAY_WIDTH - 1;
    }

    if (y < 0) {
        y = 0;
    } else if (y >= DISPLAY_HEIGHT) {
        y = DISPLAY_HEIGHT - 1;
    }

    point.touched = true;
    point.x = x;
    point.y = y;

    const uint8_t clear = 0;
    writeRegister(GT911_STATUS_REG, &clear, 1);
    return true;
}

bool TouchController::readRegister(uint16_t reg, uint8_t* data, size_t length) {
    Wire.beginTransmission(address);
    Wire.write(static_cast<uint8_t>(reg >> 8));
    Wire.write(static_cast<uint8_t>(reg & 0xFF));
    if (Wire.endTransmission(false) != 0) {
        return false;
    }

    const size_t read = Wire.requestFrom(static_cast<int>(address), static_cast<int>(length));
    if (read != length) {
        return false;
    }

    for (size_t i = 0; i < length; ++i) {
        data[i] = Wire.read();
    }
    return true;
}

bool TouchController::writeRegister(uint16_t reg, const uint8_t* data, size_t length) {
    Wire.beginTransmission(address);
    Wire.write(static_cast<uint8_t>(reg >> 8));
    Wire.write(static_cast<uint8_t>(reg & 0xFF));
    for (size_t i = 0; i < length; ++i) {
        Wire.write(data[i]);
    }
    return Wire.endTransmission() == 0;
}
