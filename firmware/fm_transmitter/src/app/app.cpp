#include "../../config.h"

#if !BRINGUP_MODE

#include "app.h"
#include "../bluetooth/bt_audio.h"
#include "../input/button.h"
#include "../display/display.h"
#include "../display/lcd_pcf8574.h"
#include "../radio/radio_service.h"
#include "../radio/si4713.h"
#include "../util/utils.h"

static Si4713 chip(Wire, PIN_SI4713_RST);
static RadioService radio(chip);
static Lcd lcdDevice(Wire1, LCD_COLS, LCD_ROWS);
static Display display(lcdDevice);
static Button lcdButton(PIN_BTN_LCD);
static BtState btState = BtState::Disconnected;
static uint32_t lastLevelLogMs = 0;
static constexpr uint32_t AUDIO_LEVEL_LOG_MS = 2000;

// "Artist - Title" for RadioText, or whichever half we have.
static void nowPlayingText(const TrackInfo &t, char *out, size_t size) {
  if (t.artist[0] && t.title[0]) snprintf(out, size, "%s - %s", t.artist, t.title);
  else if (t.title[0]) strlcpy(out, t.title, size);
  else strlcpy(out, t.artist[0] ? t.artist : RDS_IDLE_TEXT, size);
}

static const char *stateName(BtState s) {
  switch (s) {
    case BtState::Streaming: return "streaming";
    case BtState::Connected: return "connected";
    default: return "disconnected";
  }
}

// While streaming, report the Si4713's audio input level every few seconds. It shows whether
// sound actually reaches the chip: near -50 dBFS means silence, a healthy signal is well above.
static void logAudioLevel() {
  if (btState != BtState::Streaming || millis() - lastLevelLogMs < AUDIO_LEVEL_LOG_MS) return;
  lastLevelLogMs = millis();
  Si4713AsqStatus asq;
  if (chip.readAsq(asq)) Serial.printf("Audio in-level %d dBFS%s\n", asq.inLevel, asq.overmod ? " OVERMOD" : "");
}

static void handleBluetoothEvents() {
  BtState state;
  if (btAudioTakeStateChange(state)) {
    btState = state;
    Serial.printf("Bluetooth: %s\n", stateName(state));
    display.showBluetooth(state);
  }

  TrackInfo track;
  if (btAudioTakeTrackUpdate(track)) {
    char text[sizeof(track.title) * 2 + 4];
    nowPlayingText(track, text, sizeof(text));
    Serial.printf("Now playing: %s\n", text);
    radio.setRadioText(text);
    display.showTrack(track.title, track.artist);
  }
}

void appSetup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nFM transmitter");

  i2cBegin();
  lcdBusBegin();
  if (display.begin()) Serial.println("LCD ready");
  else Serial.println("LCD not found - carrying on without it");
  display.showRadio(FM_FREQ_DEFAULT, RDS_STATION_NAME);
  lcdButton.begin();

  if (radio.begin(FM_FREQ_DEFAULT, RDS_STATION_NAME)) {
    Serial.print("Transmitting on ");
    printFreq(Serial, radio.frequency());
    Serial.println();
  } else {
    Serial.println("Si4713 not found - will keep retrying");
  }

  btAudioBegin();
  Serial.printf("Bluetooth device: \"%s\"\n", BT_DEVICE_NAME);
}

void appLoop() {
  radio.tick();
  handleBluetoothEvents();
  logAudioLevel();
  display.tick();
  if (lcdButton.pressed()) {
    display.togglePower();
    Serial.printf("LCD %s\n", display.powered() ? "on" : "off");
  }
  delay(20);
}

#endif
