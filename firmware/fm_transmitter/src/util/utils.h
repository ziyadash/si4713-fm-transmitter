#pragma once

#include <Arduino.h>
#include <Wire.h>

// Start the shared I2C bus (Si4713 + LCD) with the pins and speed from config.h.
void i2cBegin();

// Start the LCD's separate I2C bus (Wire1) on PIN_LCD_SDA / PIN_LCD_SCL.
void lcdBusBegin();

// Print every address that acknowledges on the bus.
void i2cScan(Print &out);

// Print a frequency given in 10 kHz units, e.g. 8810 -> "88.10 MHz".
void printFreq(Print &out, uint16_t freq);

// Copy text into out as printable ASCII for RDS. Control characters are dropped and
// each multi-byte UTF-8 character becomes a single '?'. Always null-terminates.
void toAscii(const char *in, char *out, size_t outSize);
