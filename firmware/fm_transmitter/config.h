#pragma once

// Bluetooth A2DP -> Si4713 FM transmitter

// 1 = serial bring-up console (radio only, no Bluetooth); 0 = full firmware.
#define BRINGUP_MODE      0

// Radio
// Frequencies are in 10 kHz units, as the Si4713 expects (8810 = 88.10 MHz).
#define FM_FREQ_DEFAULT   8970    // 89.70 MHz: the quietest channel in the Sydney scan (re-run `scan` to check)
#define FM_FREQ_MIN       8750
#define FM_FREQ_MAX       10800
#define FM_FREQ_STEP      10      // 100 kHz channel spacing

// Output level in dBuV (chip range 88..115). 88 is the minimum; 100 is +12 dB for the range
// test. Australian low-power FM rules are strict and I couldn't confirm the exact limit:
// check ACMA before leaving it high, and keep it low for anything beyond bench testing.
#define FM_TX_POWER_DBUV  100

// 0 = mono (a mono receiver gains range: no stereo pilot or difference channel to add hiss),
// 1 = stereo. RDS stays on either way, though some receivers want the pilot to decode it.
#define FM_STEREO         1

// Pre-emphasis: Australia uses 50 us. The chip defaults to the US 75 us.
#define FM_PREEMPHASIS_50US 1

// Audio signal-quality (ASQ) flag thresholds, dBFS and ms. Diagnostic only.
#define ASQ_LEVEL_LOW_DBFS     -40   // below this for ASQ_DURATION_LOW_MS = "too quiet"
#define ASQ_DURATION_LOW_MS    500
#define ASQ_LEVEL_HIGH_DBFS    -6    // above this for ASQ_DURATION_HIGH_MS = "too hot"
#define ASQ_DURATION_HIGH_MS   50

#define RDS_STATION_NAME  "ZIYAD FM"        // PS name, 8 chars max
#define RDS_IDLE_TEXT     "FM Transmitter"  // RadioText when nothing is playing
#define RDS_PI_CODE       0x40A7            // programme identifier (chip default)

// Radio health check: how often to confirm the Si4713 is still tuned, and recover if not.
#define RADIO_HEALTH_CHECK_MS  1000

// Audio
#define AUDIO_BACKEND_INTERNAL_DAC 0   // GPIO25/26 8-bit DAC -> LIN/RIN via attenuator
#define AUDIO_BACKEND_I2S          1   // PCM5102A I2S DAC -> Si4713 3.5mm jack
#define AUDIO_BACKEND AUDIO_BACKEND_INTERNAL_DAC

// Fixed digital level for the A2DP stream, ignoring the phone's volume slider so
// the FM level doesn't swing with it (4096 = unchanged, lower = quieter). With the
// 8-bit DAC keep it at 4096 and attenuate with the series resistors; with the
// PCM5102A (2.1 Vrms full scale, Si4713 input maxes at 636 mVpk) lower it ~14 dB.
#define A2DP_FIXED_VOLUME 4096

#define BT_DEVICE_NAME    "FM Transmitter"

// Pins
// ESP32-A2DP v1.8.x defaults to BCK14/WS15/DATA22, so set these explicitly.
#define PIN_I2S_BCK       26
#define PIN_I2S_WS        25
#define PIN_I2S_DATA      22
// (Internal DAC pins are fixed by the chip: DAC1 = GPIO25, DAC2 = GPIO26.)

#define PIN_I2C_SDA       21
#define PIN_I2C_SCL       19
#define I2C_CLOCK_HZ      100000   // Si4713 allows 400k; the LCD backpack is a 100k part

// PWM test-tone outputs (bring-up only). Any free GPIO works; these are on the right-hand header.
#define PIN_TONE_L        18       // -> LIN
#define PIN_TONE_R        23       // -> RIN

#define PIN_SI4713_RST    4        // Si4713 GP1 is sampled at reset: leave it unconnected

#define PIN_BTN_LCD       0        // the dev board's BOOT button: LCD on/off. Tap while running;
                                   // don't hold it while the board resets (that enters flash mode).
#define PIN_BTN_UP        32       // buttons to GND, internal pull-ups
#define PIN_BTN_DOWN      33

// LCD (2004A, HD44780 behind a PCF8574 backpack)
// The LCD has its own I2C bus (Wire1) so its wiring and pull-ups stay off the Si4713's.
// Avoid GPIO12 for I2C: it is a boot-strapping pin and a pull-up there stops the board booting.
#define PIN_LCD_SDA       13
#define PIN_LCD_SCL       14
#define LCD_I2C_CLOCK_HZ  100000   // the PCF8574 is a 100 kHz part
#define LCD_I2C_ADDR      0        // 0 = try 0x27, then 0x3F
#define LCD_COLS          20
#define LCD_ROWS          4
#define LCD_SCROLL_STEP_MS 450     // how fast long titles scroll
