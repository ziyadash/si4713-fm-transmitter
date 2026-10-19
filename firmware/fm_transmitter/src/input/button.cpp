#include "button.h"

void Button::begin() {
  pinMode(_pin, INPUT_PULLUP);
  _rawLevel = _stableLevel = digitalRead(_pin);
}

bool Button::pressed() {
  bool level = digitalRead(_pin);
  uint32_t now = millis();
  if (level != _rawLevel) {
    _rawLevel = level;
    _changedMs = now;  // still bouncing: restart the timer
    return false;
  }
  if (level != _stableLevel && now - _changedMs >= _debounceMs) {
    _stableLevel = level;
    return level == LOW;  // report the press, not the release
  }
  return false;
}
