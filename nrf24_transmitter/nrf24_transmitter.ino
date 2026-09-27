/* =========================================================================
   Preo RQ77-14W (HW2181 SOC) - Serial-Driven nRF24L01 Test Transmitter
   =========================================================================
   Talks to the DRONE'S STOCK flight controller (unmodified) by emulating
   its native XN297-style radio protocol using a plain nRF24L01+ module.
   This is "Path B": keep the drone side completely stock, replace only
   the missing original remote.

   This version is driven entirely over the Serial console (115200 baud, no
   line ending needed -- newline '\n' terminates a command) instead of
   physical joysticks, so you can test/tune each layer independently before
   wiring up real controls. Type HELP once connected for the command list.

   ---- What's independently verified (high confidence) ----
   - Packet is 8 bytes: [TYPE, PITCH, ROLL, THROTTLE, YAW, BUTTONS, ID_HI, ID_LO]
     (RADIO_PACKET_SPEC.md). TYPE: 0x01=pairing, 0x02=data.
   - Stick scaling: drone computes Internal = (RawByte * 4) - 400.
     Confirmed by hand-disassembling the exact instructions at firmware
     offset 0x25a2 (lsls #2, then two immediate subs of 255+145=400 --
     Thumb's 8-bit-immediate limit forces the split, a real compiler
     artifact you wouldn't expect a fabricated report to reproduce).
   - Hop channel: ((ID_HI + ID_LO) & 0x1F) + 40. Confirmed twice in
     disassembly (0x2518, 0x2578: add, then lsls #27 / lsrs #27, which is
     the standard ARMv6-M idiom for "& 0x1F" since Thumb-1 has no
     immediate-AND). ALSO independently confirmed against real stored
     data in the firmware: bytes at file offset 0x8FF8 are literally
     `51 35 50 69 01 CC 00 00`, and (0x01+0xCC)&0x1F + 40 = 0x35 exactly
     matches the stored channel byte.

   ---- What I found myself (not in any of the markdown docs) ----
   - RF address: the real HW2181 datasheet's own worked example gives the
     chip's factory-default pipe addresses: PIPE0 = 0xE7E7E7E7E7E7,
     PIPE1 = 0xC2C2C2C2C2C2 (yes, the same 0xE7E7E7E7E7 that is *also*
     the well-known Nordic nRF24L01 factory-default address -- this SOC
     appears to be register-compatible with nRF24 by design). No
     disassembly evidence was found that the firmware ever reprograms
     registers 0x40-0x47 (the pipe address registers), so PIPE0's address
     is almost certainly left at its default.
   - BUT: the firmware DOES reprogram PKTCTRL (register 0x20) to 0x4800.
     Decoded against the datasheet's bitfield table, that sets the
     address/syncword length to 32 bits (not the 48-bit default). So the
     actual on-air address is the address truncated to 4 bytes:
     0xE7E7E7E7.
   - FREQBASE register resets to 0x0962 = 2402 (MHz), not 2400. A plain
     nRF24L01's channel numbering assumes a 2400MHz base, so "channel 3"
     in THIS chip's own numbering (2402+3=2405MHz) corresponds to
     **channel 5** on a standard nRF24L01. None of the markdown docs
     caught this; it would have caused silent failure.

   ---- What is UNVERIFIED / a real experiment ----
   - "XN297 compatibility" (bit-reversed address+payload, no Enhanced
     ShockBurst PCF field) is a well-known, independently-documented
     technique in the RC hobby "multiprotocol module" community for
     talking to XN297-family chips with plain nRF24L01+ hardware -- but
     it was not verified against THIS specific firmware's disassembly.
   - CRC: datasheet confirms CRC16 poly x^16+x^12+x^5+1 (same polynomial
     nRF24L01's own hardware CRC16 uses) but the INITIAL SEED value used
     by this firmware's software wasn't found. Default below is 16-bit
     hardware CRC; use the CRC command to try 0/8/16 at runtime.
   - PKTCTRL 0x4800 also sets a 4-bit "trailer" field that nRF24L01
     hardware has no equivalent control for at all -- there is no way to
     make a plain nRF24L01 emit a custom trailer. If the receiver is
     strict about this, no amount of address/CRC tuning will fix it.
   ========================================================================= */

#include <SPI.h>
#include <RF24.h>

#define PIN_CE   9
#define PIN_CSN  10

RF24 radio(PIN_CE, PIN_CSN);

// ---- Protocol parameters ----
#define PAIR_CHANNEL_HW2181   3     // native chip's own channel numbering
#define FREQBASE_OFFSET_MHZ   2     // HW2181 FREQBASE=2402MHz vs nRF24's 2400MHz assumption

// Deliberately NOT 0x01/0xCC: that value already exists in the drone's
// flash as a prior real pairing record (found by coincidence while
// verifying the hop-channel formula against the firmware dump), so using
// it would make "did pairing actually happen" unobservable -- the stored
// signature would already match regardless of whether our packets ever
// got through. This new ID makes a real pairing event unmistakable, since
// (0x11+0x22)&0x1F + 40 = 0x3B, distinct from the existing 0x35.
const uint8_t CONTROLLER_ID_HI = 0x11;
const uint8_t CONTROLLER_ID_LO = 0x22;

uint8_t opChannelHW2181() {
  return ((CONTROLLER_ID_HI + CONTROLLER_ID_LO) & 0x1F) + 40;
}

// ---- Runtime-tunable state (all changeable over serial, see HELP) ----
uint8_t addr[5] = {0xE7, 0xE7, 0xE7, 0xE7, 0xE7};
uint8_t addrWidth = 4;
int      curChannel   = PAIR_CHANNEL_HW2181 + FREQBASE_OFFSET_MHZ;
rf24_crclength_e crcMode = RF24_CRC_16;
rf24_datarate_e  dataRate = RF24_250KBPS;
bool useLengthHeader = true;    // byte0=0x07 length header (verified) vs byte0=type (old theory)
bool bitReversalEnabled = true; // XN297-style bit reversal on/off

// stored stick/button bytes, sent by FLY/PAIR loops until changed with SET
uint8_t stickPitch = 128, stickRoll = 128, stickThrottle = 0, stickYaw = 128, stickButtons = 0;

// manual (software) CRC16 append -- see crc16_ccitt() below for why.
bool manualCrc = false;
uint16_t manualCrcSeed = 0xFFFF;

enum Mode { IDLE, PAIRING, FLYING };
Mode mode = IDLE;

// ---- SWEEP: cycle channel x CRC x rate automatically, watching for a
// reaction is on the human, this just walks the parameter space so nobody
// has to type dozens of commands by hand. ----
bool sweepActive = false;
uint8_t sweepIndex = 0;
unsigned long sweepStepStart = 0;
const unsigned long SWEEP_STEP_MS = 1500;
const int sweepChannelsHW2181[] = {2, 3, 4, 5, 6};
const uint8_t N_SWEEP_CH = 5;
const rf24_crclength_e sweepCrcs[] = {RF24_CRC_16, RF24_CRC_DISABLED};
const uint8_t N_SWEEP_CRC = 2;
const rf24_datarate_e sweepRates[] = {RF24_250KBPS, RF24_1MBPS};
const uint8_t N_SWEEP_RATE = 2;
const uint8_t N_SWEEP_TOTAL = N_SWEEP_CH * N_SWEEP_CRC * N_SWEEP_RATE;

void applySweepStep() {
  uint8_t idx = sweepIndex;
  uint8_t chI = idx % N_SWEEP_CH; idx /= N_SWEEP_CH;
  uint8_t crcI = idx % N_SWEEP_CRC; idx /= N_SWEEP_CRC;
  uint8_t rateI = idx % N_SWEEP_RATE;

  curChannel = sweepChannelsHW2181[chI] + FREQBASE_OFFSET_MHZ;
  crcMode = sweepCrcs[crcI];
  dataRate = sweepRates[rateI];
  radio.setChannel(curChannel);
  radio.setCRCLength(crcMode);
  radio.setDataRate(dataRate);

  Serial.print(F("[SWEEP "));
  Serial.print(sweepIndex + 1);
  Serial.print('/');
  Serial.print(N_SWEEP_TOTAL);
  Serial.print(F("] native_ch=")); Serial.print(sweepChannelsHW2181[chI]);
  Serial.print(F(" nrf_ch=")); Serial.print(curChannel);
  Serial.print(F(" crc=")); Serial.print(crcMode == RF24_CRC_16 ? "16" : "off");
  Serial.print(F(" rate=")); Serial.println(dataRate == RF24_250KBPS ? "250k" : "1M");
}

// ---- XN297-style bit reversal (MSB-first vs nRF's native LSB-first) ----
uint8_t reverseBits(uint8_t b) {
  b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
  b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
  b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
  return b;
}
uint8_t maybeReverse(uint8_t b) { return bitReversalEnabled ? reverseBits(b) : b; }

void applyAddress() {
  uint8_t rev[5];
  for (uint8_t i = 0; i < addrWidth; i++) rev[i] = maybeReverse(addr[i]);
  radio.openWritingPipe(rev);
}

void applyRadioConfig() {
  bool beginOk = radio.begin();
  Serial.print(F("radio.begin(): "));
  Serial.println(beginOk ? "OK" : "FAILED");
  radio.setAutoAck(false);
  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(dataRate);
  radio.setAddressWidth(addrWidth);
  radio.setPayloadSize(8);
  radio.setRetries(0, 0);
  radio.setCRCLength(crcMode);
  radio.setChannel(curChannel);
  applyAddress();
  radio.stopListening();
}

void sendPacketRaw(uint8_t* payload, bool verbose = true) {
  uint8_t full[10];
  uint8_t len = 8;
  for (uint8_t i = 0; i < 8; i++) full[i] = payload[i];
  if (manualCrc) {
    uint16_t crc = crc16_ccitt(payload, 8, manualCrcSeed);
    full[8] = crc >> 8;
    full[9] = crc & 0xFF;
    len = 10;
  }
  uint8_t rev[10];
  for (uint8_t i = 0; i < len; i++) rev[i] = maybeReverse(full[i]);
  bool ok = radio.write(rev, len);
  if (!verbose) return;
  Serial.print(ok ? "TX ok  : " : "TX FAIL: ");
  for (uint8_t i = 0; i < len; i++) { if (full[i] < 0x10) Serial.print('0'); Serial.print(full[i], HEX); Serial.print(' '); }
  Serial.println();
}

// Disassembly of the actual receive routine shows byte 0 is read first and
// then used directly as the length for reading the rest of the payload
// (ldrb r2,[r4,#0] then RF_Read(..., count=r2)) -- it's a length header, not
// a TYPE field like RADIO_PACKET_SPEC.md assumed. Default format now
// matches that: byte0=0x07 (7 more bytes follow), no separate type byte.
// PKTFMT lets you flip back to the old TYPE-byte theory to A/B test.

void buildStoredPacket(uint8_t* pkt, uint8_t type) {
  if (useLengthHeader) {
    pkt[0] = 0x07;
    pkt[1] = stickPitch;
    pkt[2] = stickRoll;
    pkt[3] = stickThrottle;
    pkt[4] = stickYaw;
    pkt[5] = stickButtons;
    pkt[6] = CONTROLLER_ID_HI;
    pkt[7] = CONTROLLER_ID_LO;
  } else {
    pkt[0] = type;
    pkt[1] = stickPitch;
    pkt[2] = stickRoll;
    pkt[3] = stickThrottle;
    pkt[4] = stickYaw;
    pkt[5] = stickButtons;
    pkt[6] = CONTROLLER_ID_HI;
    pkt[7] = CONTROLLER_ID_LO;
  }
}

// ---- Manual CRC16 (poly 0x1021, i.e. x^16+x^12+x^5+1 -- confirmed in the
// real datasheet as this chip's CRC). The receiver hard-gates on CRC
// (confirmed: it checks the PRX_CRC_ERR0 status bit and discards the packet
// before touching payload contents if it fails), so if the seed nRF24's own
// hardware CRC uses doesn't match this chip's software seed, EVERY packet
// gets silently dropped regardless of anything else being right. This lets
// you bypass nRF24's hardware CRC and compute+append your own with a
// chosen seed instead. ----
uint16_t crc16_ccitt(uint8_t* data, uint8_t len, uint16_t seed) {
  uint16_t crc = seed;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (uint8_t b = 0; b < 8; b++) {
      crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
    }
  }
  return crc;
}

void printStatus() {
  Serial.println(F("---- status ----"));
  Serial.print(F("mode: "));       Serial.println(mode == IDLE ? "IDLE" : mode == PAIRING ? "PAIRING" : "FLYING");
  Serial.print(F("channel (nRF24 numbering): ")); Serial.println(curChannel);
  Serial.print(F("  (HW2181-native would be: ")); Serial.print(curChannel - FREQBASE_OFFSET_MHZ); Serial.println(")");
  Serial.print(F("addr: "));
  for (uint8_t i = 0; i < 4; i++) { if (addr[i] < 0x10) Serial.print('0'); Serial.print(addr[i], HEX); Serial.print(' '); }
  Serial.println();
  Serial.print(F("crc: "));  Serial.println(crcMode == RF24_CRC_DISABLED ? "disabled" : crcMode == RF24_CRC_8 ? "8-bit" : "16-bit");
  Serial.print(F("rate: ")); Serial.println(dataRate == RF24_250KBPS ? "250kbps" : dataRate == RF24_1MBPS ? "1Mbps" : "2Mbps");
  Serial.print(F("sticks P/R/T/Y/B: "));
  Serial.print(stickPitch); Serial.print('/'); Serial.print(stickRoll); Serial.print('/');
  Serial.print(stickThrottle); Serial.print('/'); Serial.print(stickYaw); Serial.print('/'); Serial.println(stickButtons);
  Serial.print(F("computed op channel (HW2181-native): "));
  Serial.println(opChannelHW2181());
  Serial.print(F("packet format: ")); Serial.println(useLengthHeader ? "byte0=0x07 length header" : "byte0=type");
  Serial.print(F("bit reversal: ")); Serial.println(bitReversalEnabled ? "ON (XN297-style)" : "OFF (native nRF24)");
  Serial.print(F("manual CRC: "));
  if (manualCrc) { Serial.print(F("ON, seed=0x")); Serial.println(manualCrcSeed, HEX); }
  else Serial.println(F("off (hardware CRC)"));
}

void printHelp() {
  Serial.println(F("---- commands ----"));
  Serial.println(F("STATUS               - show current config"));
  Serial.println(F("CHIPCHECK             - verify SPI link to the nRF24 module itself"));
  Serial.println(F("PAIR                 - continuously send TYPE=0x01 pairing packets"));
  Serial.println(F("FLY                   - continuously send TYPE=0x02 data packets"));
  Serial.println(F("STOP                  - stop continuous sending / sweeping"));
  Serial.println(F("SWEEP                 - auto-cycle channel x CRC x rate (pairing packets), ~1.5s/step"));
  Serial.println(F("PKTFMT LEN|TYPE       - byte0 = 0x07 length header (default) or type byte"));
  Serial.println(F("REV <0|1>             - toggle XN297-style bit reversal (1=on/default, 0=off)"));
  Serial.println(F("MCRC OFF | <hex seed> - disable, or compute+append CRC16 manually with given seed (e.g. MCRC FFFF)"));
  Serial.println(F("SEND                  - send one data packet immediately (any mode)"));
  Serial.println(F("SET T|P|R|Y|B <0-255> - update a stored stick/button byte"));
  Serial.println(F("CH <n>                - set nRF24 channel directly (nRF24 numbering, 2400+n MHz)"));
  Serial.println(F("OPCH                  - jump to the computed operational channel"));
  Serial.println(F("PAIRCH                - jump to the pairing channel"));
  Serial.println(F("CRC <0|1|2>           - 0=disabled 1=8-bit 2=16-bit"));
  Serial.println(F("RATE <250|1000|2000>  - set data rate (kbps)"));
  Serial.println(F("ADDR <6/8/10 hex chars> - set 3-5 byte address, e.g. ADDR E7E7E7E7"));
  Serial.println(F("AWIDTH <3|4|5>        - change address width, keeping current bytes"));
  Serial.println(F("RAW <16 hex chars>    - send one exact 8-byte packet, bypassing stored sticks"));
  Serial.println(F("HELP                  - this list"));
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  applyRadioConfig();
  Serial.println(F("nRF24 XN297-emulation test TX ready. Type HELP for commands."));
  printStatus();
}

String line;

void handleCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;
  cmd.toUpperCase();

  if (cmd == "HELP") { printHelp(); return; }
  if (cmd == "STATUS") { printStatus(); return; }
  if (cmd == "CHIPCHECK") {
    bool connected = radio.isChipConnected();
    Serial.print(F("isChipConnected(): "));
    Serial.println(connected ? "YES" : "NO -- check wiring/power");
    if (connected) {
      Serial.print(F("radio.getDataRate() readback: ")); Serial.println(radio.getDataRate());
      Serial.print(F("radio.getCRCLength() readback: ")); Serial.println(radio.getCRCLength());
      Serial.print(F("radio.getPALevel() readback: ")); Serial.println(radio.getPALevel());
    }
    return;
  }
  if (cmd == "PAIR") { mode = PAIRING; Serial.println(F("-> PAIRING")); return; }
  if (cmd == "FLY") { mode = FLYING; Serial.println(F("-> FLYING")); return; }
  if (cmd == "STOP") { mode = IDLE; sweepActive = false; Serial.println(F("-> IDLE")); return; }
  if (cmd == "SWEEP") {
    sweepActive = true;
    sweepIndex = 0;
    sweepStepStart = millis();
    Serial.println(F("-> SWEEP: cycling channel x CRC x rate, ~1.5s each. Watch the drone LED."));
    applySweepStep();
    return;
  }
  if (cmd == "OPCH") {
    curChannel = opChannelHW2181() + FREQBASE_OFFSET_MHZ;
    radio.setChannel(curChannel);
    Serial.print(F("channel -> ")); Serial.println(curChannel);
    return;
  }
  if (cmd == "PAIRCH") {
    curChannel = PAIR_CHANNEL_HW2181 + FREQBASE_OFFSET_MHZ;
    radio.setChannel(curChannel);
    Serial.print(F("channel -> ")); Serial.println(curChannel);
    return;
  }
  if (cmd == "SEND") {
    uint8_t pkt[8];
    buildStoredPacket(pkt, mode == PAIRING ? 0x01 : 0x02);
    sendPacketRaw(pkt);
    return;
  }
  if (cmd.startsWith("SET ")) {
    char axis = cmd.charAt(4);
    int val = cmd.substring(6).toInt();
    val = constrain(val, 0, 255);
    switch (axis) {
      case 'T': stickThrottle = val; break;
      case 'P': stickPitch = val; break;
      case 'R': stickRoll = val; break;
      case 'Y': stickYaw = val; break;
      case 'B': stickButtons = val; break;
      default: Serial.println(F("unknown axis, use T/P/R/Y/B")); return;
    }
    Serial.print(F("set ")); Serial.print(axis); Serial.print(F(" = ")); Serial.println(val);
    return;
  }
  if (cmd.startsWith("CH ")) {
    curChannel = constrain(cmd.substring(3).toInt(), 0, 125);
    radio.setChannel(curChannel);
    Serial.print(F("channel -> ")); Serial.println(curChannel);
    return;
  }
  if (cmd.startsWith("CRC ")) {
    int v = cmd.substring(4).toInt();
    crcMode = v == 0 ? RF24_CRC_DISABLED : v == 1 ? RF24_CRC_8 : RF24_CRC_16;
    radio.setCRCLength(crcMode);
    Serial.println(F("crc updated"));
    return;
  }
  if (cmd.startsWith("RATE ")) {
    int v = cmd.substring(5).toInt();
    dataRate = v == 250 ? RF24_250KBPS : v == 1000 ? RF24_1MBPS : RF24_2MBPS;
    radio.setDataRate(dataRate);
    Serial.println(F("rate updated"));
    return;
  }
  if (cmd.startsWith("REV ")) {
    bitReversalEnabled = cmd.substring(4).toInt() != 0;
    applyAddress(); // address bytes need re-sending through the new mode too
    Serial.print(F("bit reversal -> "));
    Serial.println(bitReversalEnabled ? "ON (XN297-style)" : "OFF (native nRF24 order)");
    return;
  }
  if (cmd.startsWith("PKTFMT ")) {
    String v = cmd.substring(7); v.trim();
    useLengthHeader = (v == "LEN");
    Serial.print(F("packet format -> "));
    Serial.println(useLengthHeader ? "byte0=0x07 length header" : "byte0=type (0x01/0x02)");
    return;
  }
  if (cmd.startsWith("MCRC ")) {
    String v = cmd.substring(5); v.trim();
    if (v == "OFF" || v == "0") {
      manualCrc = false;
      radio.setPayloadSize(8);
      radio.setCRCLength(crcMode);
      Serial.println(F("manual CRC off, using hardware CRC + 8-byte payload"));
    } else {
      manualCrcSeed = strtoul(v.c_str(), nullptr, 16);
      manualCrc = true;
      radio.setPayloadSize(10);
      radio.setCRCLength(RF24_CRC_DISABLED); // avoid double CRC (ours + hardware's)
      Serial.print(F("manual CRC on, seed=0x")); Serial.print(manualCrcSeed, HEX);
      Serial.println(F(", 10-byte payload, hardware CRC disabled"));
    }
    return;
  }
  if (cmd.startsWith("ADDR ")) {
    String hex = cmd.substring(5);
    hex.trim();
    uint8_t n = hex.length() / 2;
    if (hex.length() % 2 != 0 || n < 3 || n > 5) { Serial.println(F("need 6/8/10 hex chars (3-5 bytes)")); return; }
    for (uint8_t i = 0; i < n; i++) {
      addr[i] = strtoul(hex.substring(i * 2, i * 2 + 2).c_str(), nullptr, 16);
    }
    addrWidth = n;
    radio.setAddressWidth(addrWidth);
    applyAddress();
    Serial.println(F("address updated"));
    return;
  }
  if (cmd.startsWith("AWIDTH ")) {
    addrWidth = constrain(cmd.substring(7).toInt(), 3, 5);
    radio.setAddressWidth(addrWidth);
    applyAddress();
    Serial.print(F("address width -> ")); Serial.println(addrWidth);
    return;
  }
  if (cmd.startsWith("RAW ")) {
    String hex = cmd.substring(4);
    hex.trim();
    if (hex.length() != 16) { Serial.println(F("need exactly 16 hex chars (8 bytes)")); return; }
    uint8_t pkt[8];
    for (uint8_t i = 0; i < 8; i++) {
      pkt[i] = strtoul(hex.substring(i * 2, i * 2 + 2).c_str(), nullptr, 16);
    }
    sendPacketRaw(pkt);
    return;
  }

  Serial.println(F("unknown command, type HELP"));
}

unsigned long lastSend = 0;

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') { handleCommand(line); line = ""; }
    else if (c != '\r') { line += c; }
  }

  if (sweepActive) {
    if (millis() - sweepStepStart >= SWEEP_STEP_MS) {
      sweepIndex++;
      if (sweepIndex >= N_SWEEP_TOTAL) {
        sweepActive = false;
        Serial.println(F("[SWEEP] done, no more combinations. If nothing reacted, the"));
        Serial.println(F("mismatch is likely address/bit-order/trailer, not channel/CRC/rate."));
      } else {
        sweepStepStart = millis();
        applySweepStep();
      }
    }
    if (sweepActive && millis() - lastSend >= 20) {
      lastSend = millis();
      uint8_t pkt[8];
      buildStoredPacket(pkt, 0x01); // pairing type throughout the sweep
      sendPacketRaw(pkt, false);
    }
    return;
  }

  if (mode != IDLE && millis() - lastSend >= 20) { // ~50Hz continuous stream
    lastSend = millis();
    uint8_t pkt[8];
    buildStoredPacket(pkt, mode == PAIRING ? 0x01 : 0x02);
    sendPacketRaw(pkt);
  }
}
