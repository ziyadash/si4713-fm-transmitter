#pragma once

#include <Arduino.h>

// A push button wired from the pin to GND (internal pull-up). Ignores contact bounce.
class Button {
 public:
  explicit Button(uint8_t pin, uint32_t debounceMs = 30) : _pin(pin), _debounceMs(debounceMs) {}

  void begin();

  // True once per press, when the button goes down. Call from loop().
  bool pressed();

 private:
  uint8_t _pin;
  uint32_t _debounceMs;
  bool _rawLevel = HIGH;     // what the pin read last time
  bool _stableLevel = HIGH;  // the level once it has stopped changing
  uint32_t _changedMs = 0;
};
