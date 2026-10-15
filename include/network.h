#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "scenes.h"

// Lamp: STA plus fallback AP. Controller: home WLAN, then lamp AP.
class Network {
public:
    explicit Network(bool lamp) : lamp_(lamp) {}
    void begin() {
        WiFi.persistent(false);
        WiFi.mode(lamp_ ? WIFI_AP_STA : WIFI_STA);
        WiFi.setSleep(false);
        started_ = millis();
        if (strlen(LAMP_WIFI_SSID)) connect(false);
        else if (lamp_) startAP();
        else connect(true);
    }
    void tick() {
        uint32_t now = millis();
        if (WiFi.status() == WL_CONNECTED) return;
        if (Lamp::due(now, started_, Config::WIFI_FALLBACK_MS)) {
            if (lamp_) startAP();
            else if (!fallback_) connect(true);
        }
        if (Lamp::due(now, lastAttempt_, Config::WIFI_RETRY_MS)) connect(fallback_);
    }
    bool ready() const { return ap_ || WiFi.status() == WL_CONNECTED; }
    bool fallback() const { return fallback_; }
private:
    bool lamp_, ap_ = false, fallback_ = false;
    uint32_t started_ = 0, lastAttempt_ = 0;
    void connect(bool fallback) {
        fallback_ = fallback;
        lastAttempt_ = millis();
        if (!lamp_ || strlen(LAMP_WIFI_SSID)) {
            WiFi.begin(fallback ? LAMP_AP_SSID : LAMP_WIFI_SSID,
                       fallback ? LAMP_AP_PASSWORD : LAMP_WIFI_PASSWORD);
        }
    }
    void startAP() {
        if (ap_) return;
        ap_ = WiFi.softAP(LAMP_AP_SSID, LAMP_AP_PASSWORD);
        if (ap_) Serial.printf("Lamp AP: %s / %s\n", LAMP_AP_SSID, WiFi.softAPIP().toString().c_str());
    }
};
