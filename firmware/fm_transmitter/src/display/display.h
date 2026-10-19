#pragma once

#include "../bluetooth/bt_audio.h"
#include "../../config.h"
#include "lcd_pcf8574.h"

// What the LCD shows (20x4):
//   89.70 MHz  ZIYAD FM       frequency and RDS station name
//   Bluetooth: streaming      connection state
//   <track title>             long text scrolls
//   <artist>
// If the LCD isn't there (or is unplugged later) the firmware carries on without it.
class Display {
 public:
  explicit Display(Lcd &lcd) : _lcd(lcd) {}

  // False if no LCD answers; tick() keeps looking for it.
  bool begin();

  void showRadio(uint16_t freq, const char *stationName);
  void showBluetooth(BtState state);
  void showTrack(const char *title, const char *artist);

  // Backlight and text both off/on. Content keeps updating underneath while off.
  void setPower(bool on);
  void togglePower() { setPower(!_powered); }
  bool powered() const { return _powered; }

  // Call from loop(): scrolls long text and reconnects a missing LCD.
  void tick();

 private:
  void redraw();
  void drawRow(uint8_t row, const char *content);

  Lcd &_lcd;
  bool _powered = true;
  uint16_t _freq = 0;
  char _station[9] = "";
  BtState _state = BtState::Disconnected;
  char _title[65] = "";
  char _artist[65] = "";
  uint16_t _scrollOffset[2] = {0, 0};  // title, artist
  uint32_t _lastScrollMs = 0;
  uint32_t _lastRetryMs = 0;
  char _shown[LCD_ROWS][LCD_COLS + 1] = {};  // what is on the glass, so only changes are sent
};
