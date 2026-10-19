#include "../../config.h"

#if !BRINGUP_MODE

#include "AudioTools.h"
#include "BluetoothA2DPSink.h"
#include "bt_audio.h"
#include "../util/utils.h"

namespace {

constexpr uint32_t TRACK_SETTLE_MS = 400;

#if AUDIO_BACKEND == AUDIO_BACKEND_INTERNAL_DAC
AnalogAudioStream audioOut;
#else
I2SStream audioOut;
#endif
BluetoothA2DPSink a2dp(audioOut);
A2DPNoVolumeControl fixedVolume(A2DP_FIXED_VOLUME);

// Written by the Bluetooth task, read from loop(): guard with a spinlock.
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
BtState state = BtState::Disconnected;
bool stateDirty = false;
TrackInfo track = {"", ""};
bool trackDirty = false;
uint32_t trackChangedMs = 0;

void setState(BtState next) {
  portENTER_CRITICAL(&lock);
  if (state != next) {
    state = next;
    stateDirty = true;
    if (next == BtState::Disconnected) {
      track.title[0] = track.artist[0] = '\0';
      trackDirty = true;
      trackChangedMs = millis();
    }
  }
  portEXIT_CRITICAL(&lock);
}

void onConnectionState(esp_a2d_connection_state_t s, void *) {
  if (s == ESP_A2D_CONNECTION_STATE_CONNECTED) setState(BtState::Connected);
  else if (s == ESP_A2D_CONNECTION_STATE_DISCONNECTED) setState(BtState::Disconnected);
}

void onAudioState(esp_a2d_audio_state_t s, void *) {
  setState(s == ESP_A2D_AUDIO_STATE_STARTED ? BtState::Streaming : BtState::Connected);
}

void onMetadata(uint8_t id, const uint8_t *text) {
  char *field = nullptr;
  if (id == ESP_AVRC_MD_ATTR_TITLE) field = track.title;
  else if (id == ESP_AVRC_MD_ATTR_ARTIST) field = track.artist;
  else return;

  char clean[sizeof(track.title)];
  toAscii((const char *)text, clean, sizeof(clean));
  portENTER_CRITICAL(&lock);
  strlcpy(field, clean, sizeof(track.title));
  trackDirty = true;
  trackChangedMs = millis();
  portEXIT_CRITICAL(&lock);
}

void startAudioOutput() {
#if AUDIO_BACKEND == AUDIO_BACKEND_I2S
  auto cfg = audioOut.defaultConfig(TX_MODE);
  cfg.pin_bck = PIN_I2S_BCK;
  cfg.pin_ws = PIN_I2S_WS;
  cfg.pin_data = PIN_I2S_DATA;
  audioOut.begin(cfg);
#endif
}

}  // namespace

void btAudioBegin() {
  startAudioOutput();
  a2dp.set_volume_control(&fixedVolume);
  a2dp.set_avrc_metadata_attribute_mask(ESP_AVRC_MD_ATTR_TITLE | ESP_AVRC_MD_ATTR_ARTIST);
  a2dp.set_avrc_metadata_callback(onMetadata);
  a2dp.set_on_connection_state_changed(onConnectionState);
  a2dp.set_on_audio_state_changed(onAudioState);
  a2dp.set_auto_reconnect(true);
  a2dp.start(BT_DEVICE_NAME);
}

bool btAudioTakeStateChange(BtState &out) {
  portENTER_CRITICAL(&lock);
  bool changed = stateDirty;
  if (changed) {
    out = state;
    stateDirty = false;
  }
  portEXIT_CRITICAL(&lock);
  return changed;
}

bool btAudioTakeTrackUpdate(TrackInfo &out) {
  bool ready = false;
  portENTER_CRITICAL(&lock);
  if (trackDirty && millis() - trackChangedMs >= TRACK_SETTLE_MS) {
    out = track;
    trackDirty = false;
    ready = true;
  }
  portEXIT_CRITICAL(&lock);
  return ready;
}

#endif
