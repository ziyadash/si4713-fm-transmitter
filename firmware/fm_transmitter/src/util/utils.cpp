#include "utils.h"
#include "../../config.h"

void i2cBegin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(I2C_CLOCK_HZ);
}

void lcdBusBegin() { Wire1.begin(PIN_LCD_SDA, PIN_LCD_SCL, LCD_I2C_CLOCK_HZ); }

void i2cScan(Print &out) {
  out.println("I2C scan:");
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) out.printf("  0x%02X\n", addr);
  }
}

void printFreq(Print &out, uint16_t freq) { out.printf("%u.%02u MHz", freq / 100, freq % 100); }

void toAscii(const char *in, char *out, size_t outSize) {
  size_t n = 0;
  for (const uint8_t *c = (const uint8_t *)in; *c && n + 1 < outSize; c++) {
    if (*c >= 0x20 && *c < 0x7F) out[n++] = *c;
    else if (*c >= 0xC0) out[n++] = '?';  // lead byte of a multi-byte character
    // continuation bytes (0x80-0xBF) and control characters are skipped
  }
  out[n] = '\0';
}
