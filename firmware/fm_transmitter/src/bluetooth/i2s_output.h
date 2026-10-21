#pragma once

#include <Arduino.h>

class I2SClass;  // from ESP_I2S.h, kept out of this header so it doesn't clash with AudioTools

// I2S audio output for the A2DP sink, on the pins from config.h. Clocks start with the first audio
// and follow the phone's sample rate; stop() switches them off when playback stops. This is the same
// ESP_I2S driver the bring-up I2S test tone uses, which is known to work with the DAC wiring.
class I2sOutput : public Print {
 public:
  void begin();                       // allocate; no clocks yet
  void setSampleRate(uint32_t rate);  // takes effect on the next write
  void stop();                        // clocks off (paused or disconnected)

  size_t write(uint8_t b) override { return write(&b, 1); }
  size_t write(const uint8_t *data, size_t size) override;

 private:
  bool start(uint32_t rate);  // caller holds the lock

  I2SClass *_i2s = nullptr;
  SemaphoreHandle_t _lock = nullptr;
  uint32_t _rate = 44100;
  uint32_t _pendingRate = 44100;
  bool _running = false;
};
