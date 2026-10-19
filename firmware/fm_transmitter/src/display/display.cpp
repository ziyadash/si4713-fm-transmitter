#include "display.h"

namespace {

constexpr uint8_t ROW_RADIO = 0;
constexpr uint8_t ROW_BLUETOOTH = 1;
constexpr uint8_t ROW_TITLE = 2;
constexpr uint8_t ROW_ARTIST = 3;

constexpr uint32_t RETRY_MS = 2000;
constexpr size_t SCROLL_GAP = 3;  // blank characters between the end of a title and its repeat

// Text longer than the row scrolls: the window starts `offset` characters in and wraps.
size_t scrollPeriod(const char *text) { return strlen(text) + SCROLL_GAP; }

void window(const char *text, uint16_t offset, char *out) {
  size_t length = strlen(text);
  if (length <= LCD_COLS) {
    strlcpy(out, text, LCD_COLS + 1);
    return;
  }
  for (size_t i = 0; i < LCD_COLS; i++) {
    size_t index = (offset + i) % scrollPeriod(text);
    out[i] = index < length ? text[index] : ' ';
  }
  out[LCD_COLS] = '\0';
}

const char *bluetoothLine(BtState state) {
  switch (state) {
    case BtState::Streaming: return "Bluetooth: streaming";
    case BtState::Connected: return "Bluetooth: connected";
    default: return "Bluetooth: waiting";
  }
}

}  // namespace

bool Display::begin() {
  memset(_shown, 0, sizeof(_shown));  // nothing is on the glass yet, so draw every row
  bool found = _lcd.begin(LCD_I2C_ADDR);
  if (found) {
    if (!_powered) _lcd.setPower(false);  // a reconnected LCD comes up on; keep it off if it was
    redraw();
  }
  return found;
}

void Display::showRadio(uint16_t freq, const char *stationName) {
  _freq = freq;
  strlcpy(_station, stationName, sizeof(_station));
  redraw();
}

void Display::showBluetooth(BtState state) {
  _state = state;
  redraw();
}

void Display::showTrack(const char *title, const char *artist) {
  strlcpy(_title, title, sizeof(_title));
  strlcpy(_artist, artist, sizeof(_artist));
  _scrollOffset[0] = _scrollOffset[1] = 0;
  redraw();
}

void Display::setPower(bool on) {
  _powered = on;
  _lcd.setPower(on);
}

void Display::tick() {
  uint32_t now = millis();
  if (!_lcd.ok()) {
    if (now - _lastRetryMs >= RETRY_MS) {
      _lastRetryMs = now;
      begin();
    }
    return;
  }

  if (!_powered || now - _lastScrollMs < LCD_SCROLL_STEP_MS) return;  // no point scrolling an off screen
  _lastScrollMs = now;
  bool scrolled = false;
  const char *texts[2] = {_title, _artist};
  for (int i = 0; i < 2; i++) {
    if (strlen(texts[i]) > LCD_COLS) {
      _scrollOffset[i] = (_scrollOffset[i] + 1) % scrollPeriod(texts[i]);
      scrolled = true;
    }
  }
  if (scrolled) redraw();
}

void Display::redraw() {
  if (!_lcd.ok()) return;

  char line[LCD_COLS + 1];
  snprintf(line, sizeof(line), "%u.%02u MHz  %s", _freq / 100, _freq % 100, _station);
  drawRow(ROW_RADIO, line);
  drawRow(ROW_BLUETOOTH, bluetoothLine(_state));

  if (_title[0] || _artist[0]) {
    window(_title, _scrollOffset[0], line);
    drawRow(ROW_TITLE, line);
    window(_artist, _scrollOffset[1], line);
    drawRow(ROW_ARTIST, line);
  } else if (_state == BtState::Disconnected) {
    drawRow(ROW_TITLE, "Pair with:");
    drawRow(ROW_ARTIST, BT_DEVICE_NAME);
  } else {
    drawRow(ROW_TITLE, "Nothing playing");
    drawRow(ROW_ARTIST, "");
  }
}

// Pad to the full row so old characters are overwritten, and skip rows that haven't changed.
void Display::drawRow(uint8_t row, const char *content) {
  char padded[LCD_COLS + 1];
  memset(padded, ' ', LCD_COLS);
  memcpy(padded, content, min(strlen(content), (size_t)LCD_COLS));
  padded[LCD_COLS] = '\0';

  if (strcmp(padded, _shown[row]) == 0) return;
  if (_lcd.writeAt(0, row, padded)) strlcpy(_shown[row], padded, sizeof(_shown[row]));
}
