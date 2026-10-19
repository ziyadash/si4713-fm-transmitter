#pragma once

#include "si4713.h"

// Keeps the Si4713 transmitting. Remembers the frequency, station name and
// RadioText, checks the chip regularly, and if it has reset (it has done this on
// the bench) brings it back with everything restored.
class RadioService {
 public:
  explicit RadioService(Si4713 &radio) : _radio(radio) {}

  // Initialise, tune and start RDS. Returns false if the chip isn't answering;
  // tick() keeps retrying in that case.
  bool begin(uint16_t freq, const char *stationName);

  // Call from loop(). Runs the health check every RADIO_HEALTH_CHECK_MS.
  void tick();

  bool setFrequency(uint16_t freq);
  void setRadioText(const char *text);

  uint16_t frequency() const { return _freq; }
  uint32_t recoveries() const { return _recoveries; }

 private:
  bool bringUp();   // reset, tune, restore RDS
  bool healthy();   // still answering, tuned and transmitting?

  Si4713 &_radio;
  uint16_t _freq = 0;
  char _station[9] = "";
  char _text[65] = "";
  uint32_t _lastCheckMs = 0;
  uint32_t _recoveries = 0;
};
