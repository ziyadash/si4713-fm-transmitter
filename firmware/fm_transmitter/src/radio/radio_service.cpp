#include "radio_service.h"
#include "../../config.h"
#include "../util/utils.h"

bool RadioService::begin(uint16_t freq, const char *stationName) {
  _freq = freq;
  strlcpy(_station, stationName, sizeof(_station));
  strlcpy(_text, RDS_IDLE_TEXT, sizeof(_text));
  return bringUp();
}

bool RadioService::bringUp() {
  if (!_radio.begin() || !_radio.tune(_freq)) return false;
  if (!_radio.hasRds()) return true;
  return _radio.beginRds(RDS_PI_CODE) && _radio.setRdsStationName(_station) &&
         _radio.setRdsText(_text);
}

bool RadioService::healthy() {
  Si4713TuneStatus st;
  return _radio.readTuneStatus(st) && st.freq == _freq && st.powerDbuv > 0;
}

void RadioService::tick() {
  if (millis() - _lastCheckMs < RADIO_HEALTH_CHECK_MS) return;
  _lastCheckMs = millis();
  if (healthy()) return;

  Serial.println("Si4713 stopped transmitting - recovering");
  if (bringUp()) {
    _recoveries++;
    Serial.printf("recovered (%u so far)\n", (unsigned)_recoveries);
  } else {
    Serial.println("recovery failed, will retry");
  }
}

bool RadioService::setFrequency(uint16_t freq) {
  if (!_radio.tune(freq)) return false;
  _freq = freq;
  return true;
}

void RadioService::setRadioText(const char *text) {
  strlcpy(_text, text, sizeof(_text));
  _radio.setRdsText(_text);  // if this fails, recovery re-sends it
}
