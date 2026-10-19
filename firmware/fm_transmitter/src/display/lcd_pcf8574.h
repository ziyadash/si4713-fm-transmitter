#pragma once

#include <Arduino.h>
#include <Wire.h>

// HD44780 character LCD behind a PCF8574 I2C backpack (the common "HW-61" board),
// driven in 4-bit mode. Takes the bus as a parameter so the LCD can live on Wire1.
//
// Assumes the usual backpack wiring: P0=RS, P1=RW, P2=E, P3=backlight, P4..P7=D4..D7.
class Lcd {
 public:
  Lcd(TwoWire &wire, uint8_t cols, uint8_t rows) : _wire(wire), _cols(cols), _rows(rows) {}

  // Find the backpack (address 0 = try 0x27 then 0x3F) and initialise the display.
  // Returns false if nothing answers. Safe to call again to recover a lost display.
  bool begin(uint8_t address = 0);

  // False after any failed write, until begin() succeeds again.
  bool ok() const { return _ok; }

  bool clear();

  // Backlight and the characters themselves. The display keeps its text while off, so
  // switching back on shows it again without a redraw.
  bool setBacklight(bool on);
  bool setDisplayOn(bool on);
  bool setPower(bool on) { return setDisplayOn(on) & setBacklight(on); }
  // Write text at a position. Anything past the end of the row is dropped.
  bool writeAt(uint8_t col, uint8_t row, const char *text);

 private:
  bool probe(uint8_t address);
  bool command(uint8_t value);
  bool writeNibble(uint8_t nibble);
  bool setCursor(uint8_t col, uint8_t row);
  bool send(const uint8_t *bytes, size_t length);
  uint8_t backlightBit() const;

  TwoWire &_wire;
  uint8_t _cols;
  uint8_t _rows;
  uint8_t _addr = 0;
  bool _ok = false;
  bool _backlight = true;
};
