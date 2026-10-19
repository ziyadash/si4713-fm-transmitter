#include <Wire.h>
#include <Adafruit_Si4713.h>

#define RESETPIN 24
#define SDA_PIN 13
#define SCL_PIN 14
#define FMSTATION 8810  // 88.10 MHz - change to an empty frequency in your area

Adafruit_Si4713 radio = Adafruit_Si4713(RESETPIN);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Adafruit Si4713 FM Transmitter Test");

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!radio.begin()) {
    Serial.println("Couldn't find Si4713 - check wiring!");
    while (1);
  }

  Serial.println("Si4713 found!");

  radio.setTXpower(115); // max power, adjust if needed
  radio.tuneFM(FMSTATION);
  delay(500);
  radio.readTuneStatus();

  Serial.print("Tuned to: ");
  Serial.println(radio.currFreq);
}

void loop() {
  radio.readASQ();
  Serial.print("Current ASQ: 0x");
  Serial.println(radio.currASQ, HEX);
  Serial.print("Current InLevel: ");
  Serial.println(radio.currInLevel);
  delay(2000);
}
