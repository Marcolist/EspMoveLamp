#pragma once
#include <stdint.h>
#include <stddef.h>

#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef LAMP_WIFI_SSID
#define LAMP_WIFI_SSID ""
#endif
#ifndef LAMP_WIFI_PASSWORD
#define LAMP_WIFI_PASSWORD ""
#endif
#ifndef LAMP_AP_SSID
#define LAMP_AP_SSID "EspMoveLamp"
#endif
#ifndef LAMP_AP_PASSWORD
#define LAMP_AP_PASSWORD "EspMove1234"
#endif
#ifndef LAMP_PAIRING_KEY
#define LAMP_PAIRING_KEY "EspMoveLamp-change-this-key"
#endif

namespace Config {
constexpr uint16_t UDP_PORT = 4210;
constexpr uint32_t WIFI_RETRY_MS = 15000;
constexpr uint32_t WIFI_FALLBACK_MS = 25000;
constexpr uint32_t DISCOVERY_MS = 1500;
constexpr uint32_t OFFLINE_MS = 5500;
constexpr uint32_t ACK_TIMEOUT_MS = 450;
constexpr uint8_t MAX_ATTEMPTS = 4;
constexpr size_t PACKET_BYTES = 512;

// Physical/data order: top, front upper, front lower, bottom.
constexpr uint8_t STRIP_TOP = 0;
constexpr uint8_t STRIP_FRONT_UPPER = 1;
constexpr uint8_t STRIP_FRONT_LOWER = 2;
constexpr uint8_t STRIP_BOTTOM = 3;
constexpr uint8_t SECTIONS = 4;
constexpr uint8_t ROWS_PER_SECTION = 1;
constexpr uint16_t LEDS_PER_ROW = 30;
constexpr uint16_t LED_COUNT = SECTIONS * ROWS_PER_SECTION * LEDS_PER_ROW;
// One chain snakes L->R, R->L, L->R, R->L.
// Four outputs each run L->R, in the same physical strip order.
constexpr uint8_t LED_OUTPUTS = 1;
constexpr uint8_t LED_PINS[4] = {18, 19, 23, 25};
constexpr bool SERPENTINE_ROWS = false;
constexpr bool reverseStrip(uint8_t strip, uint8_t outputs = LED_OUTPUTS) {
    return outputs == 1 && (strip & 1);
}
constexpr uint8_t INITIAL_BRIGHTNESS = 60;  // 0..255
constexpr uint32_t LED_CURRENT_LIMIT_MA = 4000; // estimate, LEDs only

constexpr uint8_t SERVO_PIN = 27;
// DS3225: 500..2500us corresponds to 180 OR 270 degrees depending on model.
// MG does not identify the angle variant. Until checked, keep the narrow
// +/-9 nominal degree limits below: they generate only 1400..1600us.
// Use 270 here if your servo is the 270-degree version. Centre follows it.
constexpr int SERVO_MIN_US = 500;
constexpr int SERVO_MAX_US = 2500;
constexpr float SERVO_RANGE_DEG = 180.0f;
constexpr float SERVO_HOME_DEG = SERVO_RANGE_DEG / 2;
constexpr float SERVO_REFERENCE_DEG = SERVO_HOME_DEG; // calibrate on the assembled lamp
constexpr float SERVO_MIN_DEG = SERVO_HOME_DEG - 9.0f;
constexpr float SERVO_MAX_DEG = SERVO_HOME_DEG + 9.0f;
static_assert(SERVO_REFERENCE_DEG >= SERVO_MIN_DEG && SERVO_REFERENCE_DEG <= SERVO_MAX_DEG,
              "Reference position must be within mechanical limits");
constexpr float SERVO_SPEED_DPS = 18.0f;
constexpr bool SERVO_REVERSED = false;
constexpr uint8_t SERVO_PWM_CHANNEL = 0;

constexpr uint8_t TFT_ROTATION = 1;
constexpr uint8_t TOUCH_ROTATION = 1;
constexpr uint8_t TOUCH_CS_PIN = 33;
constexpr uint8_t TOUCH_IRQ_PIN = 36;
constexpr uint8_t TOUCH_CLK_PIN = 25;
constexpr uint8_t TOUCH_MISO_PIN = 39;
constexpr uint8_t TOUCH_MOSI_PIN = 32;
constexpr uint16_t TOUCH_PRESSURE_MIN = 300;
constexpr uint8_t DISPLAY_BACKLIGHT = 180;
// Optional static address if UDP broadcasts are blocked; otherwise leave empty.
constexpr char LAMP_IP[] = "";

static_assert(LED_OUTPUTS == 1 || LED_OUTPUTS == SECTIONS,
              "Use one chain or one chain per section (up to 4)");
static_assert(SECTIONS > 0 && SECTIONS <= 4 && ROWS_PER_SECTION > 0 && LEDS_PER_ROW > 0,
              "Invalid LED geometry");
static_assert(SERVO_MIN_DEG >= 0 && SERVO_MAX_DEG <= SERVO_RANGE_DEG &&
              SERVO_MIN_DEG < SERVO_MAX_DEG && SERVO_HOME_DEG >= SERVO_MIN_DEG &&
              SERVO_HOME_DEG <= SERVO_MAX_DEG, "Invalid servo limits");
static_assert(SERVO_MIN_US >= 500 && SERVO_MAX_US <= 2500 && SERVO_MIN_US < SERVO_MAX_US,
              "Invalid servo pulses");
static_assert(sizeof(LAMP_AP_PASSWORD) >= 9, "AP password needs at least 8 characters");
}
