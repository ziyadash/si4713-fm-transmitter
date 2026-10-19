#pragma once

#include <Arduino.h>

// Bluetooth A2DP sink: receives music from the phone and plays it out of the DAC
// (or I2S, per AUDIO_BACKEND). Also reports connection state and the track title
// and artist (AVRCP). Events are picked up from loop() with the "take" functions,
// so nothing here touches the I2C bus from the Bluetooth task.

enum class BtState : uint8_t { Disconnected, Connected, Streaming };

struct TrackInfo {
  char title[64];
  char artist[64];
};

void btAudioBegin();

// True once each time the connection or streaming state changes.
bool btAudioTakeStateChange(BtState &state);

// True once per track change, after the title/artist have settled (they arrive as
// separate messages).
bool btAudioTakeTrackUpdate(TrackInfo &track);
