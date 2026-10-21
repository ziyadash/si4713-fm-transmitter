#include "si4713.h"
#include "../../config.h"

namespace {

// Commands
constexpr uint8_t CMD_POWER_UP = 0x01;
constexpr uint8_t CMD_GET_REV = 0x10;
constexpr uint8_t CMD_POWER_DOWN = 0x11;
constexpr uint8_t CMD_SET_PROPERTY = 0x12;
constexpr uint8_t CMD_GET_PROPERTY = 0x13;
constexpr uint8_t CMD_GET_INT_STATUS = 0x14;
constexpr uint8_t CMD_TX_TUNE_FREQ = 0x30;
constexpr uint8_t CMD_TX_TUNE_POWER = 0x31;
constexpr uint8_t CMD_TX_TUNE_MEASURE = 0x32;
constexpr uint8_t CMD_TX_TUNE_STATUS = 0x33;
constexpr uint8_t CMD_TX_ASQ_STATUS = 0x34;
constexpr uint8_t CMD_TX_RDS_BUFF = 0x35;
constexpr uint8_t CMD_TX_RDS_PS = 0x36;

// Properties (datasheet Table 18)
constexpr uint16_t PROP_REFCLK_FREQ = 0x0201;
constexpr uint16_t PROP_TX_COMPONENT_ENABLE = 0x2100;
constexpr uint16_t PROP_TX_AUDIO_DEVIATION = 0x2101;
constexpr uint16_t PROP_TX_RDS_DEVIATION = 0x2103;
constexpr uint16_t PROP_TX_LINE_INPUT_LEVEL = 0x2104;
constexpr uint16_t PROP_TX_PREEMPHASIS = 0x2106;
constexpr uint16_t PROP_TX_ACOMP_ENABLE = 0x2200;
constexpr uint16_t PROP_TX_ASQ_INTERRUPT_SOURCE = 0x2300;
constexpr uint16_t PROP_TX_ASQ_LEVEL_LOW = 0x2301;
constexpr uint16_t PROP_TX_ASQ_DURATION_LOW = 0x2302;
constexpr uint16_t PROP_TX_ASQ_LEVEL_HIGH = 0x2303;
constexpr uint16_t PROP_TX_ASQ_DURATION_HIGH = 0x2304;
constexpr uint16_t PROP_TX_RDS_PI = 0x2C01;
constexpr uint16_t PROP_TX_RDS_PS_MIX = 0x2C02;
constexpr uint16_t PROP_TX_RDS_PS_MISC = 0x2C03;
constexpr uint16_t PROP_TX_RDS_PS_REPEAT_COUNT = 0x2C04;
constexpr uint16_t PROP_TX_RDS_PS_MESSAGE_COUNT = 0x2C05;
constexpr uint16_t PROP_TX_RDS_PS_AF = 0x2C06;
constexpr uint16_t PROP_TX_RDS_FIFO_SIZE = 0x2C07;

constexpr uint8_t STATUS_CTS = 0x80;
constexpr uint8_t STATUS_STCINT = 0x01;

constexpr uint8_t ADDR_SEN_HIGH = 0x63;  // breakout default
constexpr uint8_t ADDR_SEN_LOW = 0x11;

constexpr uint8_t POWER_UP_FM_TX_XOSC = 0x12;      // XOSCEN | FUNC = FM transmit
constexpr uint8_t POWER_UP_ANALOG_INPUT = 0x50;    // OPMODE = analog audio in
constexpr uint32_t POWER_UP_SETTLE_MS = 120;       // datasheet: up to 110 ms
constexpr uint16_t REFCLK_HZ = 32768;
constexpr uint16_t ASQ_ALL_SOURCES = 0x0007;       // low, high, overmod
constexpr uint8_t POWER_MIN_DBUV = 88;
constexpr uint8_t POWER_MAX_DBUV = 115;

// Deviations are in 10 Hz steps. The total must stay within 75 kHz: audio + pilot + RDS.
constexpr uint16_t MAX_DEVIATION = 7500;
constexpr uint16_t PILOT_DEVIATION = 675;  // chip default, 6.75 kHz
constexpr uint16_t RDS_DEVIATION = 200;    // 2 kHz
constexpr uint16_t COMPONENT_PILOT = 0x0001;
constexpr uint16_t COMPONENT_STEREO_DIFF = 0x0002;
constexpr uint16_t COMPONENT_RDS = 0x0004;
constexpr size_t RDS_PS_LENGTH = 8;
constexpr size_t RDS_TEXT_LENGTH = 64;
constexpr uint8_t RDS_BUFF_LOAD = 0x04;   // LDBUFF: append to the group buffer
constexpr uint8_t RDS_BUFF_CLEAR = 0x02;  // MTBUFF: empty the buffer first
constexpr uint8_t RDS_GROUP_2A_B_HIGH = 0x20;  // block B high byte: group 2A (RadioText)

}  // namespace

Si4713::Si4713(TwoWire &wire, int8_t rstPin) : _wire(wire), _rst(rstPin) {}

// Start-up

bool Si4713::begin() {
  hardwareReset();
  return findAddress() && powerUpFm() && readPartNumber() && applyDefaults();
}

void Si4713::hardwareReset() {
  if (_rst < 0) return;
  // GP1/GP2 stay floating so the chip selects 2-wire mode on the rising edge.
  pinMode(_rst, OUTPUT);
  digitalWrite(_rst, HIGH);
  delay(10);
  digitalWrite(_rst, LOW);
  delay(10);
  digitalWrite(_rst, HIGH);
  delay(10);
}

bool Si4713::findAddress() {
  if (probe(ADDR_SEN_HIGH)) _addr = ADDR_SEN_HIGH;
  else if (probe(ADDR_SEN_LOW)) _addr = ADDR_SEN_LOW;
  else return false;
  return true;
}

bool Si4713::powerUpFm() {
  const uint8_t cmd[] = {CMD_POWER_UP, POWER_UP_FM_TX_XOSC, POWER_UP_ANALOG_INPUT};
  if (!sendCommand(cmd, sizeof(cmd))) return false;
  delay(POWER_UP_SETTLE_MS);
  return true;
}

bool Si4713::readPartNumber() {
  const uint8_t cmd[] = {CMD_GET_REV, 0x00};
  uint8_t rev[9];
  if (!sendCommand(cmd, sizeof(cmd), rev, sizeof(rev))) return false;
  _part = rev[1];
  return _part == 12 || _part == 13;
}

bool Si4713::applyDefaults() {
  _powerDbuv = FM_TX_POWER_DBUV;  // applied by the first tune()
  _stereo = FM_STEREO;
  _rdsEnabled = false;
  bool ok = setProperty(PROP_REFCLK_FREQ, REFCLK_HZ);
  ok &= updateModulation();
  ok &= setPreemphasis(FM_PREEMPHASIS_50US);
  ok &= setLimiter(true, false);  // Adafruit's library writes 0 here (limiter off)
  ok &= setLineInput(3, 636);     // datasheet default: 636 mVpk, 60 kohm

  // Enable the audio signal-quality flags so ASQ status reports something.
  ok &= setProperty(PROP_TX_ASQ_INTERRUPT_SOURCE, ASQ_ALL_SOURCES);
  ok &= setProperty(PROP_TX_ASQ_LEVEL_LOW, (uint16_t)(int16_t)ASQ_LEVEL_LOW_DBFS);
  ok &= setProperty(PROP_TX_ASQ_DURATION_LOW, ASQ_DURATION_LOW_MS);
  ok &= setProperty(PROP_TX_ASQ_LEVEL_HIGH, (uint16_t)(int16_t)ASQ_LEVEL_HIGH_DBFS);
  ok &= setProperty(PROP_TX_ASQ_DURATION_HIGH, ASQ_DURATION_HIGH_MS);
  return ok;
}

// Tuning

bool Si4713::tune(uint16_t freq) {
  const uint8_t cmd[] = {CMD_TX_TUNE_FREQ, 0x00, (uint8_t)(freq >> 8), (uint8_t)freq};
  Si4713TuneStatus st;
  if (!sendCommand(cmd, sizeof(cmd)) || !finishTune(st)) return false;
  // Re-tune the antenna capacitor for the new frequency.
  return setPower(_powerDbuv, 0);
}

bool Si4713::setPower(uint8_t dbuv, uint8_t antCap) {
  if (dbuv != 0) dbuv = constrain(dbuv, POWER_MIN_DBUV, POWER_MAX_DBUV);
  _powerDbuv = dbuv;
  const uint8_t cmd[] = {CMD_TX_TUNE_POWER, 0x00, 0x00, dbuv, antCap};
  Si4713TuneStatus st;
  return sendCommand(cmd, sizeof(cmd)) && finishTune(st);
}

bool Si4713::measureNoise(uint16_t freq, uint8_t &noiseDbuv) {
  freq -= freq % 5;  // must be a multiple of 50 kHz
  const uint8_t cmd[] = {CMD_TX_TUNE_MEASURE, 0x00, (uint8_t)(freq >> 8), (uint8_t)freq, 0x00};
  Si4713TuneStatus st;
  if (!sendCommand(cmd, sizeof(cmd)) || !finishTune(st)) return false;
  noiseDbuv = st.noise;
  return true;
}

bool Si4713::readTuneStatus(Si4713TuneStatus &st) {
  const uint8_t cmd[] = {CMD_TX_TUNE_STATUS, 0x01};  // INTACK
  uint8_t r[8];
  if (!sendCommand(cmd, sizeof(cmd), r, sizeof(r))) return false;
  st.freq = ((uint16_t)r[2] << 8) | r[3];
  st.powerDbuv = r[5];
  st.antCap = r[6];
  st.noise = r[7];
  return true;
}

bool Si4713::finishTune(Si4713TuneStatus &st) { return waitStc() && readTuneStatus(st); }

// Audio

bool Si4713::readAsq(Si4713AsqStatus &st) {
  const uint8_t cmd[] = {CMD_TX_ASQ_STATUS, 0x01};  // INTACK
  uint8_t r[5];
  if (!sendCommand(cmd, sizeof(cmd), r, sizeof(r))) return false;
  st.rawLatched = r[1];
  st.rawCurrent = r[2];
  st.inLevel = (int8_t)r[4];
  st.lowLevel = r[2] & 0x01;
  st.highLevel = r[2] & 0x02;
  st.overmod = r[2] & 0x04;
  return true;
}

bool Si4713::setStereo(bool stereo) {
  _stereo = stereo;
  return updateModulation();
}

bool Si4713::updateModulation() {
  uint16_t components = (_stereo ? COMPONENT_PILOT | COMPONENT_STEREO_DIFF : 0) |
                        (_rdsEnabled ? COMPONENT_RDS : 0);
  uint16_t audioDeviation = MAX_DEVIATION - (_stereo ? PILOT_DEVIATION : 0) -
                            (_rdsEnabled ? RDS_DEVIATION : 0);
  return setProperty(PROP_TX_AUDIO_DEVIATION, audioDeviation) &&
         setProperty(PROP_TX_COMPONENT_ENABLE, components);
}

bool Si4713::readModulation(uint16_t &components, uint16_t &audioDeviation) {
  return getProperty(PROP_TX_COMPONENT_ENABLE, components) &&
         getProperty(PROP_TX_AUDIO_DEVIATION, audioDeviation);
}

bool Si4713::setPreemphasis(bool us50) {
  return setProperty(PROP_TX_PREEMPHASIS, us50 ? 1 : 0);
}

bool Si4713::setLimiter(bool on, bool compressor) {
  uint16_t v = (on ? 0x0002 : 0) | (compressor ? 0x0001 : 0);
  return setProperty(PROP_TX_ACOMP_ENABLE, v);
}

bool Si4713::setLineInput(uint8_t attenCode, uint16_t peakMv) {
  uint16_t v = ((uint16_t)(attenCode & 0x3) << 12) | (peakMv & 0x03FF);
  return setProperty(PROP_TX_LINE_INPUT_LEVEL, v);
}

// Properties and power

bool Si4713::setProperty(uint16_t prop, uint16_t value) {
  const uint8_t cmd[] = {CMD_SET_PROPERTY, 0x00, (uint8_t)(prop >> 8), (uint8_t)prop,
                         (uint8_t)(value >> 8), (uint8_t)value};
  return sendCommand(cmd, sizeof(cmd));
}

bool Si4713::getProperty(uint16_t prop, uint16_t &value) {
  const uint8_t cmd[] = {CMD_GET_PROPERTY, 0x00, (uint8_t)(prop >> 8), (uint8_t)prop};
  uint8_t r[4];
  if (!sendCommand(cmd, sizeof(cmd), r, sizeof(r))) return false;
  value = ((uint16_t)r[2] << 8) | r[3];
  return true;
}

bool Si4713::powerDown() {
  const uint8_t cmd[] = {CMD_POWER_DOWN};
  return sendCommand(cmd, sizeof(cmd));
}

// RDS (Si4713 only)

bool Si4713::beginRds(uint16_t piCode) {
  if (!hasRds()) return false;
  _rdsEnabled = true;
  bool ok = updateModulation();
  ok &= setProperty(PROP_TX_RDS_DEVIATION, RDS_DEVIATION);
  ok &= setProperty(PROP_TX_RDS_PI, piCode);
  ok &= setProperty(PROP_TX_RDS_PS_MIX, 0x0003);
  ok &= setProperty(PROP_TX_RDS_PS_MISC, 0x1008);
  ok &= setProperty(PROP_TX_RDS_PS_REPEAT_COUNT, 3);
  ok &= setProperty(PROP_TX_RDS_PS_MESSAGE_COUNT, 1);
  ok &= setProperty(PROP_TX_RDS_PS_AF, 0xE0E0);  // no alternative frequency
  ok &= setProperty(PROP_TX_RDS_FIFO_SIZE, 0);
  return ok;
}

// The 8-character PS name is sent as two 4-character slots.
bool Si4713::setRdsStationName(const char *name) {
  if (!hasRds()) return false;
  char padded[RDS_PS_LENGTH];
  memset(padded, ' ', sizeof(padded));
  memcpy(padded, name, min(strlen(name), RDS_PS_LENGTH));
  for (uint8_t slot = 0; slot < RDS_PS_LENGTH / 4; slot++) {
    uint8_t cmd[6] = {CMD_TX_RDS_PS, slot};
    memcpy(cmd + 2, padded + slot * 4, 4);
    if (!sendCommand(cmd, sizeof(cmd))) return false;
  }
  return true;
}

// RadioText goes out as group 2A, four characters per group. Text shorter than 64
// characters is ended with a carriage return, as the RDS standard requires.
bool Si4713::setRdsText(const char *text) {
  if (!hasRds()) return false;
  char chars[RDS_TEXT_LENGTH];
  memset(chars, ' ', sizeof(chars));
  size_t len = min(strlen(text), RDS_TEXT_LENGTH);
  memcpy(chars, text, len);
  size_t used = len;
  if (len < RDS_TEXT_LENGTH) chars[used++] = '\r';

  _rtAbFlag = !_rtAbFlag;
  size_t segments = (used + 3) / 4;
  for (size_t seg = 0; seg < segments; seg++) {
    uint8_t flags = RDS_BUFF_LOAD | (seg == 0 ? RDS_BUFF_CLEAR : 0);
    uint8_t blockBLow = (_rtAbFlag ? 0x10 : 0x00) | (uint8_t)seg;
    uint8_t cmd[8] = {CMD_TX_RDS_BUFF, flags, RDS_GROUP_2A_B_HIGH, blockBLow};
    memcpy(cmd + 4, chars + seg * 4, 4);
    if (!sendCommand(cmd, sizeof(cmd))) return false;
  }
  return true;
}

// I2C transport

bool Si4713::probe(uint8_t addr) {
  _wire.beginTransmission(addr);
  return _wire.endTransmission() == 0;
}

bool Si4713::readStatus(uint8_t &status) {
  if (_wire.requestFrom((int)_addr, 1) != 1) return false;
  status = _wire.read();
  return true;
}

bool Si4713::waitCts(uint32_t timeoutMs) {
  uint32_t start = millis();
  uint8_t status;
  while (millis() - start < timeoutMs) {
    if (readStatus(status) && (status & STATUS_CTS)) return true;
    delay(1);
  }
  return false;
}

// Poll GET_INT_STATUS until the tune-complete flag is set.
bool Si4713::waitStc(uint32_t timeoutMs) {
  const uint8_t cmd = CMD_GET_INT_STATUS;
  uint8_t resp[1];
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    if (sendCommand(&cmd, 1, resp, 1) && (resp[0] & STATUS_STCINT)) return true;
    delay(5);
  }
  return false;
}

bool Si4713::sendCommand(const uint8_t *cmd, uint8_t len, uint8_t *resp, uint8_t respLen) {
  _wire.beginTransmission(_addr);
  _wire.write(cmd, len);
  if (_wire.endTransmission() != 0 || !waitCts()) return false;
  if (respLen == 0) return true;
  if (_wire.requestFrom((int)_addr, (int)respLen) != respLen) return false;
  for (uint8_t i = 0; i < respLen; i++) resp[i] = _wire.read();
  return true;
}
