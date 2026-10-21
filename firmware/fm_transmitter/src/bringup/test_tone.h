#pragma once

#include <Arduino.h>

// Test tones for checking the analog audio path with no Bluetooth involved.
// Both stop with testToneStop(); starting one replaces any tone already playing.

// Sine from the ESP32's built-in DAC on GPIO25 and GPIO26. attenCode: 0..3 =
// 0/6/12/18 dB below full scale. Returns false on failure.
bool testToneStart(uint32_t hz, uint8_t attenCode);

// 3.3 V 50% PWM square wave on PIN_TONE_L / PIN_TONE_R (config.h). A harsher buzz
// than the sine, but usable on any GPIO. Level is set by the series resistors.
bool testToneStartPwm(uint32_t hz);

// Sine on the I2S pins from config.h (BCK, WS, DATA), for checking the DAC wiring with no
// Bluetooth involved. Quiet on purpose (about -20 dBFS).
bool testToneStartI2s(uint32_t hz);

void testToneStop();
