#include "lcd_pcf8574.h"

namespace {

// PCF8574 pin assignments on the backpack
constexpr uint8_t PIN_RS = 0x01;
constexpr uint8_t PIN_EN = 0x04;
constexpr uint8_t PIN_BACKLIGHT = 0x08;

// HD44780 commands
constexpr uint8_t CMD_CLEAR = 0x01;
constexpr uint8_t CMD_ENTRY_MODE_INCREMENT = 0x06;
constexpr uint8_t CMD_DISPLAY_OFF = 0x08;
constexpr uint8_t CMD_DISPLAY_ON = 0x0C;        // no cursor, no blink
constexpr uint8_t CMD_FUNCTION_4BIT_2LINE = 0x28;
constexpr uint8_t CMD_SET_DDRAM = 0x80;

constexpr size_t MAX_COLS = 20;  // one I2C transaction per row must fit Wire's 128-byte buffer

// The character ROM draws a backslash as a yen sign and a tilde as an arrow.
char romSafe(char c) {
  if (c == '\\') return '/';
  if (c == '~') return '-';
  return c;
}

}  // namespace

uint8_t Lcd::backlightBit() const { return _backlight ? PIN_BACKLIGHT : 0; }

bool Lcd::probe(uint8_t address) {
  _wire.beginTransmission(address);
  return _wire.endTransmission() == 0;
}

bool Lcd::send(const uint8_t *bytes, size_t length) {
  _wire.beginTransmission(_addr);
  _wire.write(bytes, length);
  bool sent = _wire.endTransmission() == 0;
  if (!sent) _ok = false;
  return sent;
}

// One nibble on D4..D7, with an E pulse (E high then low) so the display latches it.
bool Lcd::writeNibble(uint8_t nibble) {
  uint8_t data = (nibble << 4) | backlightBit();
  const uint8_t bytes[] = {(uint8_t)(data | PIN_EN), data};
  return send(bytes, sizeof(bytes));
}

bool Lcd::command(uint8_t value) {
  uint8_t hi = (value & 0xF0) | backlightBit();
  uint8_t lo = ((value << 4) & 0xF0) | backlightBit();
  const uint8_t bytes[] = {(uint8_t)(hi | PIN_EN), hi, (uint8_t)(lo | PIN_EN), lo};
  return send(bytes, sizeof(bytes));
}

bool Lcd::begin(uint8_t address) {
  _ok = false;
  if (address) _addr = address;
  else if (probe(0x27)) _addr = 0x27;
  else if (probe(0x3F)) _addr = 0x3F;
  else return false;
  if (!probe(_addr)) return false;

  // HD44780 "initialise by instruction": 8-bit mode three times, then switch to 4-bit.
  _ok = true;
  delay(50);
  writeNibble(0x3);
  delay(5);
  writeNibble(0x3);
  delayMicroseconds(150);
  writeNibble(0x3);
  delayMicroseconds(150);
  writeNibble(0x2);

  command(CMD_FUNCTION_4BIT_2LINE);  // also correct for 4-line modules
  command(CMD_DISPLAY_OFF);
  clear();
  command(CMD_ENTRY_MODE_INCREMENT);
  command(CMD_DISPLAY_ON);
  return _ok;
}

bool Lcd::setBacklight(bool on) {
  _backlight = on;
  const uint8_t bytes[] = {backlightBit()};  // E and RS low, so the display ignores it
  return send(bytes, sizeof(bytes));
}

bool Lcd::setDisplayOn(bool on) { return command(on ? CMD_DISPLAY_ON : CMD_DISPLAY_OFF); }

bool Lcd::clear() {
  bool done = command(CMD_CLEAR);
  delay(2);  // clear takes about 1.5 ms
  return done;
}

// Rows are not contiguous in display memory: 0x00, 0x40, then the same offsets
// continued after the first two rows' worth of columns.
bool Lcd::setCursor(uint8_t col, uint8_t row) {
  const uint8_t rowOffset[4] = {0x00, 0x40, (uint8_t)_cols, (uint8_t)(0x40 + _cols)};
  return command(CMD_SET_DDRAM | (rowOffset[row] + col));
}

bool Lcd::writeAt(uint8_t col, uint8_t row, const char *text) {
  if (!_ok || row >= _rows || col >= _cols) return false;
  size_t length = min(strlen(text), (size_t)(_cols - col));
  if (!setCursor(col, row)) return false;

  // Whole row in one I2C transaction: four bytes (two E pulses) per character.
  uint8_t bytes[4 * MAX_COLS];
  length = min(length, MAX_COLS);
  for (size_t i = 0; i < length; i++) {
    uint8_t c = romSafe(text[i]);
    uint8_t hi = (c & 0xF0) | PIN_RS | backlightBit();
    uint8_t lo = ((c << 4) & 0xF0) | PIN_RS | backlightBit();
    bytes[i * 4 + 0] = hi | PIN_EN;
    bytes[i * 4 + 1] = hi;
    bytes[i * 4 + 2] = lo | PIN_EN;
    bytes[i * 4 + 3] = lo;
  }
  return send(bytes, length * 4);
}
