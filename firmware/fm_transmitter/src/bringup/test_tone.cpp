#include "test_tone.h"
#include "../../config.h"
#include "driver/dac_cosine.h"
#include "ESP_I2S.h"

static dac_cosine_handle_t channels[2] = {nullptr, nullptr};
static bool pwmActive = false;

static I2SClass i2s;
static volatile bool i2sRunning = false;
static volatile bool i2sTaskDone = true;
static uint32_t i2sToneHz = 1000;

// Streams a quiet stereo sine to the I2S output until i2sRunning is cleared.
static void i2sToneTask(void *) {
  constexpr uint32_t sampleRate = 44100;
  constexpr float amplitude = 3500.0f;  // out of 32767
  constexpr int frames = 256;
  static int16_t buf[frames * 2];
  float phase = 0.0f;
  while (i2sRunning) {
    const float step = 2.0f * PI * i2sToneHz / sampleRate;
    for (int i = 0; i < frames; i++) {
      int16_t v = (int16_t)(amplitude * sinf(phase));
      buf[2 * i] = v;
      buf[2 * i + 1] = v;
      phase += step;
      if (phase > 2.0f * PI) phase -= 2.0f * PI;
    }
    i2s.write((uint8_t *)buf, sizeof(buf));  // blocks until the DMA has room: this paces the loop
  }
  i2sTaskDone = true;
  vTaskDelete(nullptr);
}

void testToneStop() {
  if (i2sRunning) {
    i2sRunning = false;
    while (!i2sTaskDone) delay(5);
    i2s.end();
  }
  for (auto &ch : channels) {
    if (!ch) continue;
    dac_cosine_stop(ch);
    dac_cosine_del_channel(ch);
    ch = nullptr;
  }
  if (pwmActive) {
    ledcDetach(PIN_TONE_L);
    ledcDetach(PIN_TONE_R);
    pinMode(PIN_TONE_L, INPUT);  // leave the pins high-impedance
    pinMode(PIN_TONE_R, INPUT);
    pwmActive = false;
  }
}

bool testToneStartI2s(uint32_t hz) {
  testToneStop();
  i2s.setPins(PIN_I2S_BCK, PIN_I2S_WS, PIN_I2S_DATA);
  if (!i2s.begin(I2S_MODE_STD, 44100, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) return false;
  i2sToneHz = hz;
  i2sRunning = true;
  i2sTaskDone = false;
  xTaskCreate(i2sToneTask, "i2sTone", 4096, nullptr, 2, nullptr);
  return true;
}

bool testToneStartPwm(uint32_t hz) {
  testToneStop();
  constexpr uint8_t resolutionBits = 8;
  constexpr uint32_t halfDuty = 1u << (resolutionBits - 1);
  if (!ledcAttach(PIN_TONE_L, hz, resolutionBits) || !ledcAttach(PIN_TONE_R, hz, resolutionBits)) {
    ledcDetach(PIN_TONE_L);
    return false;
  }
  ledcWrite(PIN_TONE_L, halfDuty);
  ledcWrite(PIN_TONE_R, halfDuty);
  pwmActive = true;
  return true;
}

bool testToneStart(uint32_t hz, uint8_t attenCode) {
  testToneStop();
  const dac_channel_t dacChannels[2] = {DAC_CHAN_0, DAC_CHAN_1};  // GPIO25, GPIO26
  for (int i = 0; i < 2; i++) {
    dac_cosine_config_t cfg = {};
    cfg.chan_id = dacChannels[i];
    cfg.freq_hz = hz;
    cfg.clk_src = DAC_COSINE_CLK_SRC_DEFAULT;
    cfg.atten = (dac_cosine_atten_t)attenCode;
    cfg.phase = DAC_COSINE_PHASE_0;
    cfg.offset = 0;
    cfg.flags.force_set_freq = true;
    if (dac_cosine_new_channel(&cfg, &channels[i]) != ESP_OK ||
        dac_cosine_start(channels[i]) != ESP_OK) {
      testToneStop();
      return false;
    }
  }
  return true;
}
