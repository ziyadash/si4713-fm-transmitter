#pragma once

#include <Arduino.h>
#include <Wire.h>

// Minimal Si4713 (FM transmitter) driver over I2C. Written against the
// Si4712/13-B30 datasheet; command/response formats follow Silicon Labs AN332
// (not in the repo), cross-checked with Adafruit's library.

struct Si4713TuneStatus {
  uint16_t freq;     // 10 kHz units (8810 = 88.10 MHz)
  uint8_t powerDbuv; // output level in dBuV
  uint8_t antCap;    // antenna tuning cap code, 0..191 (5 pF + 0.25 pF per step)
  uint8_t noise;     // received noise level (dBuV), valid after a measure
};

struct Si4713AsqStatus {
  uint8_t rawLatched; // RESP1: latched event flags (cleared when read)
  uint8_t rawCurrent; // RESP2: current condition flags
  int8_t inLevel;     // input level in dBFS
  // Decoded from rawCurrent. Bit layout per AN332 (memory): 0 = level below the
  // low threshold, 1 = level above the high threshold, 2 = overmodulation.
  bool lowLevel;
  bool highLevel;
  bool overmod;
};

class Si4713 {
 public:
  Si4713(TwoWire &wire, int8_t rstPin);

  // Reset, find the chip (0x63 then 0x11), power up in analog-input FM-TX mode
  // and apply the default configuration. Returns false if no Si4712/13 answers.
  bool begin();

  uint8_t address() const { return _addr; }
  uint8_t partNumber() const { return _part; }   // 12 = Si4712, 13 = Si4713
  bool hasRds() const { return _part == 13; }

  // Tune to freq (10 kHz units) and re-tune the antenna cap at the current power.
  bool tune(uint16_t freq);
  // Output level in dBuV (clamped to 88..115, 0 = carrier off). antCap 0 = auto.
  bool setPower(uint8_t dbuv, uint8_t antCap = 0);
  bool readTuneStatus(Si4713TuneStatus &st);
  // Measure received noise on a channel (freq must be a multiple of 50 kHz).
  // This moves the synthesiser: call tune() again before transmitting.
  bool measureNoise(uint16_t freq, uint8_t &noiseDbuv);

  bool readAsq(Si4713AsqStatus &st);

  // Mono drops the 19 kHz pilot and the stereo difference channel: better signal-to-noise on
  // a weak signal, and the freed deviation goes to the audio. Audio is L+R either way.
  bool setStereo(bool stereo);
  bool setPreemphasis(bool us50);                 // false = 75 us (US default)
  bool setLimiter(bool on, bool compressor = false);
  // Peak input voltage the line input should expect. attenCode is LIATTEN[1:0]
  // (0..3 = 190/301/416/636 mV peak, 396/100/74/60 kohm input resistance).
  bool setLineInput(uint8_t attenCode, uint16_t peakMv);

  // RDS (Si4713 only). beginRds() enables the subcarrier and trims the audio deviation
  // so audio + pilot + RDS stay within 75 kHz. Station name (PS) is up to 8 characters,
  // RadioText up to 64. Call these again after any chip reset.
  bool beginRds(uint16_t piCode);
  bool setRdsStationName(const char *name);
  bool setRdsText(const char *text);

  // What the chip is actually set to (for logging): multiplex components mask and audio deviation.
  bool readModulation(uint16_t &components, uint16_t &audioDeviation);

  bool setProperty(uint16_t prop, uint16_t value);
  bool getProperty(uint16_t prop, uint16_t &value);
  bool powerDown();

 private:
  // begin() steps
  void hardwareReset();
  bool findAddress();
  bool powerUpFm();
  bool readPartNumber();
  bool applyDefaults();
  bool updateModulation();  // component mask and audio deviation for the current stereo/RDS state

  // I2C transport
  bool probe(uint8_t addr);
  bool sendCommand(const uint8_t *cmd, uint8_t len, uint8_t *resp = nullptr, uint8_t respLen = 0);
  bool readStatus(uint8_t &status);
  bool waitCts(uint32_t timeoutMs = 300);
  bool waitStc(uint32_t timeoutMs = 1500);
  bool finishTune(Si4713TuneStatus &st);  // wait for tune complete, then read (and clear) it

  TwoWire &_wire;
  int8_t _rst;
  uint8_t _addr = 0;
  uint8_t _part = 0;
  uint8_t _powerDbuv = 88;
  bool _stereo = true;
  bool _rdsEnabled = false;
  bool _rtAbFlag = false;  // RadioText A/B flag: flips on each new text so receivers redraw
};
