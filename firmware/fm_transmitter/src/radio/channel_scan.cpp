#include "channel_scan.h"
#include "../../config.h"

static const size_t kChannelCount = (FM_FREQ_MAX - FM_FREQ_MIN) / FM_FREQ_STEP + 1;

size_t findQuietChannels(Si4713 &radio, uint16_t returnToFreq, QuietChannel *out, size_t count) {
  uint8_t noise[kChannelCount];
  for (size_t i = 0; i < kChannelCount; i++) {
    if (!radio.measureNoise(FM_FREQ_MIN + i * FM_FREQ_STEP, noise[i])) {
      radio.tune(returnToFreq);
      return 0;
    }
  }
  radio.tune(returnToFreq);

  bool taken[kChannelCount] = {};
  size_t found = 0;
  for (; found < count && found < kChannelCount; found++) {
    size_t best = kChannelCount;
    for (size_t i = 0; i < kChannelCount; i++) {
      if (!taken[i] && (best == kChannelCount || noise[i] < noise[best])) best = i;
    }
    taken[best] = true;
    out[found] = {(uint16_t)(FM_FREQ_MIN + best * FM_FREQ_STEP), noise[best]};
  }
  return found;
}
