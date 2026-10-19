#pragma once

#include "si4713.h"

struct QuietChannel {
  uint16_t freq;  // 10 kHz units
  uint8_t noise;  // received noise level, dBuV (lower = emptier channel)
};

// Measure received noise across FM_FREQ_MIN..FM_FREQ_MAX and fill `out` with the
// `count` quietest channels, quietest first. Measuring moves the synthesiser, so
// the radio is re-tuned to `returnToFreq` before returning. Returns how many
// channels were written (0 if a measurement failed). Takes roughly 16 seconds.
size_t findQuietChannels(Si4713 &radio, uint16_t returnToFreq, QuietChannel *out, size_t count);
