#include "test_tone.h"
#include "../../config.h"
#include "driver/dac_cosine.h"

static dac_cosine_handle_t channels[2] = {nullptr, nullptr};
static bool pwmActive = false;

void testToneStop() {
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
