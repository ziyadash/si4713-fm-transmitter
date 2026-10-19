#include <stdlib.h>
#include <string.h>
#include "bringup.h"
#include "../radio/channel_scan.h"
#include "../../config.h"
#include "../radio/si4713.h"
#include "test_tone.h"
#include "../util/utils.h"

namespace {

Si4713 radio(Wire, PIN_SI4713_RST);
bool radioOk = false;
uint16_t currentFreq = FM_FREQ_DEFAULT;

// Output helpers

void printTuneStatus() {
  Si4713TuneStatus st;
  if (!radio.readTuneStatus(st)) {
    Serial.println("tune status read failed");
    return;
  }
  Serial.print("  freq ");
  printFreq(Serial, st.freq);
  // Datasheet Table 9 gives the tuning cap as 5 pF (code 0) to 53 pF (code 191), i.e. 5 pF + 0.25 pF/step.
  Serial.printf(", power %u dBuV, antenna cap %u (~%.1f pF)\n", st.powerDbuv, st.antCap,
                5.0f + st.antCap * 0.25f);
}

void printAsq() {
  Si4713AsqStatus a;
  if (!radio.readAsq(a)) {
    Serial.println("ASQ read failed");
    return;
  }
  Serial.printf("  in-level %d dBFS  raw latched 0x%02X current 0x%02X  [%s%s%s]\n", a.inLevel,
                a.rawLatched, a.rawCurrent, a.lowLevel ? "LOW " : "", a.highLevel ? "HIGH " : "",
                a.overmod ? "OVERMOD" : "");
}

void printOk(bool ok) { Serial.println(ok ? "ok" : "failed"); }

// Radio control

bool tuneTo(float mhz) {
  uint16_t freq = (uint16_t)(mhz * 100.0f + 0.5f);
  if (freq < FM_FREQ_MIN || freq > FM_FREQ_MAX) {
    Serial.println("frequency out of range (87.50-108.00)");
    return false;
  }
  if (!radio.tune(freq)) {
    Serial.println("tune failed");
    return false;
  }
  currentFreq = freq;
  printTuneStatus();
  return true;
}

void startRadio() {
  radioOk = radio.begin();
  if (!radioOk) {
    Serial.println("Si4713 not found - check wiring (SDA/SCL/RST/power)");
    return;
  }
  Serial.printf("Si47%u at 0x%02X\n", radio.partNumber(), radio.address());
  tuneTo(FM_FREQ_DEFAULT / 100.0f);
}

// Commands
// Each handler gets the (up to three) arguments after the command name; unused
// slots are nullptr. The dispatcher has already checked minArgs.

using Args = const char *const *;

void cmdHelp(Args);

void cmdInfo(Args) {
  Serial.printf("Si47%u at 0x%02X\n", radio.partNumber(), radio.address());
  printTuneStatus();
}

void cmdTune(Args a) { tuneTo(atof(a[0])); }

void cmdPower(Args a) {
  printOk(radio.setPower(atoi(a[0])));
  printTuneStatus();
}

void cmdScan(Args) {
  Serial.println("Scanning 87.5-108.0 MHz, about 16 s...");
  QuietChannel best[5];
  size_t n = findQuietChannels(radio, currentFreq, best, 5);
  if (n == 0) {
    Serial.println("scan failed");
    return;
  }
  Serial.println("Quietest channels (lower dBuV = emptier):");
  for (size_t i = 0; i < n; i++) {
    Serial.print("  ");
    printFreq(Serial, best[i].freq);
    Serial.printf("  %u dBuV\n", best[i].noise);
  }
}

void cmdAsq(Args) { printAsq(); }

void cmdWatch(Args) {
  Serial.println("watching ASQ, send any key to stop");
  while (!Serial.available()) {
    printAsq();
    delay(1000);
  }
  while (Serial.available()) Serial.read();
}

void cmdTone(Args a) {
  const char *mode = a[0];
  if (!strcmp(mode, "off")) {
    testToneStop();
    Serial.println("tone off");
    return;
  }
  uint32_t hz = a[1] ? atoi(a[1]) : 1000;
  if (!strcmp(mode, "on")) {
    uint8_t atten = a[2] ? atoi(a[2]) : 3;
    if (testToneStart(hz, atten)) Serial.printf("DAC sine on GPIO25/26: %u Hz, atten code %u\n", (unsigned)hz, atten);
    else Serial.println("tone start failed");
  } else if (!strcmp(mode, "pwm")) {
    if (testToneStartPwm(hz)) Serial.printf("PWM square on GPIO%d/GPIO%d: %u Hz\n", PIN_TONE_L, PIN_TONE_R, (unsigned)hz);
    else Serial.println("tone start failed");
  } else {
    Serial.println("usage: tone <on|pwm|off> [Hz] [0-3]");
  }
}

void cmdPre(Args a) { printOk(radio.setPreemphasis(atoi(a[0]) == 50)); }
void cmdStereo(Args a) { printOk(radio.setStereo(!strcmp(a[0], "on"))); }
void cmdLim(Args a) { printOk(radio.setLimiter(!strcmp(a[0], "on"))); }
void cmdLine(Args a) { printOk(radio.setLineInput(atoi(a[0]), atoi(a[1]))); }
void cmdReinit(Args) { startRadio(); }

struct Command {
  const char *name;
  const char *usage;
  const char *help;
  uint8_t minArgs;
  bool needsRadio;
  void (*run)(Args);
};

const Command commands[] = {
    {"info", "info", "chip and tune status", 0, true, cmdInfo},
    {"tune", "tune <MHz>", "tune (e.g. tune 88.1) and re-tune the antenna", 1, true, cmdTune},
    {"power", "power <dBuV>", "88..115, 0 = carrier off", 1, true, cmdPower},
    {"scan", "scan", "noise scan, lists the quietest channels", 0, true, cmdScan},
    {"asq", "asq", "one audio-signal-quality reading", 0, true, cmdAsq},
    {"watch", "watch", "ASQ once a second, any key stops", 0, true, cmdWatch},
    {"tone", "tone <on|pwm|off> [Hz] [0-3]", "on = DAC sine GPIO25/26 (0-3 = 0/6/12/18 dB down); pwm = square on GPIO18/23", 1, false, cmdTone},
    {"pre", "pre <50|75>", "pre-emphasis in us", 1, true, cmdPre},
    {"stereo", "stereo <on|off>", "stereo or mono (off) transmission", 1, true, cmdStereo},
    {"lim", "lim <on|off>", "audio limiter", 1, true, cmdLim},
    {"line", "line <0-3> <mV>", "line-input attenuation code and expected peak mV", 2, true, cmdLine},
    {"reinit", "reinit", "reset and power the chip up again", 0, false, cmdReinit},
    {"help", "help", "this list", 0, false, cmdHelp},
};

void cmdHelp(Args) {
  Serial.println("Commands:");
  for (const Command &c : commands) Serial.printf("  %-30s %s\n", c.usage, c.help);
}

// Split "name a b c" into the command and up to three arguments, then dispatch.
void runLine(char *line) {
  const char *args[4] = {};
  int count = 0;
  for (char *tok = strtok(line, " "); tok && count < 4; tok = strtok(nullptr, " ")) args[count++] = tok;
  if (count == 0) return;

  for (const Command &c : commands) {
    if (strcmp(args[0], c.name) != 0) continue;
    if (count - 1 < c.minArgs) Serial.printf("usage: %s\n", c.usage);
    else if (c.needsRadio && !radioOk) Serial.println("radio not initialised (try: reinit)");
    else c.run(args + 1);
    return;
  }
  Serial.println("unknown command (try: help)");
}

}  // namespace

void bringupSetup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nFM transmitter - bring-up console");
  i2cBegin();
  i2cScan(Serial);
  startRadio();
  cmdHelp(nullptr);
}

void bringupLoop() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  char buf[64];
  strlcpy(buf, line.c_str(), sizeof(buf));
  runLine(buf);
}
