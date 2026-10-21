#include "i2s_output.h"
#include "../../config.h"
#include "ESP_I2S.h"

void I2sOutput::begin() {
  if (_i2s) return;
  _i2s = new I2SClass();
  _lock = xSemaphoreCreateMutex();
}

void I2sOutput::setSampleRate(uint32_t rate) { _pendingRate = rate; }

bool I2sOutput::start(uint32_t rate) {
  _i2s->setPins(PIN_I2S_BCK, PIN_I2S_WS, PIN_I2S_DATA);
  return _i2s->begin(I2S_MODE_STD, rate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
}

void I2sOutput::stop() {
  if (!_i2s) return;
  xSemaphoreTake(_lock, portMAX_DELAY);
  if (_running) {
    _i2s->end();
    _running = false;
  }
  xSemaphoreGive(_lock);
}

// Called from the Bluetooth task with 16-bit stereo samples. write() blocks until the DMA has room,
// which paces the stream.
size_t I2sOutput::write(const uint8_t *data, size_t size) {
  if (!_i2s) return size;
  xSemaphoreTake(_lock, portMAX_DELAY);
  if (_running && _pendingRate != _rate) {  // the phone changed sample rate: restart the clocks
    _i2s->end();
    _running = false;
  }
  if (!_running) {
    _rate = _pendingRate;
    _running = start(_rate);
  }
  size_t written = _running ? _i2s->write((uint8_t *)data, size) : size;  // drop audio if I2S won't start
  xSemaphoreGive(_lock);
  return written;
}
