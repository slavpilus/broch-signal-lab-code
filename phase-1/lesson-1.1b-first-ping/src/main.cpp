/*
 * Broch Signal — Phase 1 · Lesson 1.1b
 * "Two boards on 868 MHz: the Discovery kit's first ping"  (receiver half)
 *
 * Raw LoRa, RX-only. Brings up the diymore "LoRa V4" board as a receiver and
 * listens for the Discovery kit's 1.1a beacon, logging RSSI and SNR for every
 * packet it hears. No LoRaWAN — this is the bare Semtech radio, one board
 * hearing another.
 *
 *   Board : diymore "LoRa V4"  ==  Heltec WiFi LoRa 32 V4 clone (HTIT-WB32LAF)
 *           ESP32-S3R2 + Semtech SX1262, EU868.
 *           *** Confirm the silkscreen reads V4 / HTIT-WB32LAF before trusting
 *               the pin map below. A true V3 uses the same LoRa pins. ***
 *   Radio : 868.1 MHz · BW 125 kHz · SF7 · CR 4/5   — identical to Lesson 1.1a.
 *
 * The whole lesson is one idea: a receiver only hears a transmitter when every
 * radio parameter matches. Change one and the V4 goes deaf. Three settings do
 * the damage in practice, and this firmware pins all three to what the 1.1a
 * firmware actually sends:
 *
 *   1. Sync word 0x12 (private). RadioLib's SX126x default happens to be 0x12
 *      too, but we set it so the match is explicit, not luck. On the SX1262
 *      this becomes the 0x1424 register pair — the standard private value,
 *      interoperable with the SX1276's single-byte 0x12 from 1.1a.
 *
 *   2. Preamble length 8 symbols — arduino-LoRa's default on the TX side.
 *
 *   3. CRC OFF. The 1.1a firmware never calls enableCrc(), so its packets
 *      carry no payload CRC. RadioLib turns CRC ON by default; leave it on and
 *      the V4 rejects every 1.1a packet as corrupt. setCRC(false) is the fix.
 *
 * Three board-specific lines the SX1262 needs that a generic example omits:
 *   - TCXO. The reference is a TCXO fed from the SX1262's DIO3 pin, so a
 *     control voltage must be supplied (TCXO_VOLTAGE below). A plain crystal
 *     would use 0.0 V. If begin() returns a negative code, sweep this value.
 *   - DIO2 drives the antenna RF switch (setDio2AsRfSwitch).
 *   - V4 clones add a GC1109 RF front-end whose enable sits on GPIO2; it must
 *     be HIGH or the receive (LNA) path is dead. Harmless to set on a V3.
 *
 * Pin map: Heltec WiFi LoRa 32 V3/V4. Confirm against your board's silkscreen.
 */

#include <Arduino.h>
#include <RadioLib.h>

// ---- SX1262 <-> ESP32-S3 pin map (Heltec WiFi LoRa 32 V3/V4) ----------------
#define LORA_NSS 8   // SPI chip-select
#define LORA_DIO1 14 // IRQ (RxDone)
#define LORA_RESET 12
#define LORA_BUSY 13
#define LORA_SCK 9
#define LORA_MISO 11
#define LORA_MOSI 10

// V4 GC1109 RF front-end enable. HIGH = front-end powered. No-op on a true V3.
#define LORA_FEM_EN 2

// On-board LED (Heltec V3/V4). Blinked once per received packet.
#define LED_PIN 35

// ---- Radio settings — every one MUST equal Lesson 1.1a ----------------------
#define LORA_FREQ_MHZ 868.1
#define LORA_BW_KHZ 125.0
#define LORA_SF 7
#define LORA_CR 5          // coding rate 4/5
#define LORA_SYNCWORD 0x12 // private network (LoRaWAN would be 0x34)
#define LORA_PREAMBLE 8
#define LORA_TX_DBM 14 // unused while RX-only; set for parity with 1.1b TX

// SX1262 TCXO control voltage on DIO3. 1.6 V is radiolib's default and the
// usual heltec value. if begin() fails with a negative code, try 1.8, then 0.0
// (0.0 = no TCXO / plain crystal). Whatever makes begin() return 0 is correct
// for your physical unit — write that value in the lab notebook.
#define TCXO_VOLTAGE 1.6

SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RESET, LORA_BUSY);

volatile bool packetReady = false; // set from the DIO1 ISR
uint32_t rxCount = 0;

// Keep the ISR tiny and in IRAM: just flag the main loop.
void IRAM_ATTR onPacket() { packetReady = true; }

// Slow blink forever — used when setup cannot continue.
void halt(const char *why, int code) {
  Serial.print(F("HALT: "));
  Serial.print(why);
  Serial.print(F("  (code "));
  Serial.print(code);
  Serial.println(F(")"));
  while (true) {
    digitalWrite(LED_PIN, HIGH);
    delay(500);
    digitalWrite(LED_PIN, LOW);
    delay(500);
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(115200);
  // Native USB-CDC on the ESP32-S3 needs a moment to enumerate; don't lose the
  // banner.
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 3000) {
    delay(10);
  }
  delay(500);
  Serial.println();
  Serial.println(
      F("Broch Signal — Lesson 1.1b — raw LoRa, RX-only (the V4 listens)"));

  // V4 RF front-end: power it before the radio expects to receive.
  pinMode(LORA_FEM_EN, OUTPUT);
  digitalWrite(LORA_FEM_EN, HIGH);

  // The Heltec LoRa pins are not the ESP32-S3 default SPI pins — set them.
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);

  // Bring up the SX1262 with every parameter matched to 1.1a's transmitter.
  Serial.print(F("radio.begin()... "));
  int st = radio.begin(LORA_FREQ_MHZ, LORA_BW_KHZ, LORA_SF, LORA_CR,
                       LORA_SYNCWORD, LORA_TX_DBM, LORA_PREAMBLE, TCXO_VOLTAGE);
  if (st != RADIOLIB_ERR_NONE) {
    halt("radio.begin failed — if negative, sweep TCXO_VOLTAGE (1.8, then 0.0)",
         st);
  }
  Serial.println(F("ok"));

  // DIO2 is wired to the antenna RF switch on this module.
  radio.setDio2AsRfSwitch(true);

  // CRC OFF to match 1.1a, which transmits no payload CRC. This is the setting
  // most likely to leave you "receiving nothing" if you forget it.
  st = radio.setCRC(false);
  if (st != RADIOLIB_ERR_NONE)
    halt("setCRC(false) failed", st);

  radio.setPacketReceivedAction(onPacket);

  Serial.println(F(
      "config: 868.1 MHz  SF7  BW125  CR4/5  sync 0x12  preamble 8  CRC off"));
  Serial.println(F("listening for \"BrochSignal 1.1a #N\" ..."));
  Serial.println();

  st = radio.startReceive();
  if (st != RADIOLIB_ERR_NONE)
    halt("startReceive failed", st);
}

void loop() {
  if (!packetReady)
    return;
  packetReady = false;

  String msg;
  int st = radio.readData(msg);

  if (st == RADIOLIB_ERR_NONE) {
    rxCount++;
    digitalWrite(LED_PIN, HIGH);
    Serial.print(F("RX #"));
    Serial.print(rxCount);
    Serial.print(F("  \""));
    Serial.print(msg);
    Serial.print(F("\"  "));
    Serial.print(msg.length());
    Serial.print(F(" bytes   RSSI "));
    Serial.print(radio.getRSSI());
    Serial.print(F(" dBm   SNR "));
    Serial.print(radio.getSNR());
    Serial.print(F(" dB   f_err "));
    Serial.print(radio.getFrequencyError());
    Serial.println(F(" Hz"));
    digitalWrite(LED_PIN, LOW);
  } else if (st == RADIOLIB_ERR_CRC_MISMATCH) {
    Serial.println(
        F("RX: CRC mismatch — TX and RX disagree on CRC (see setCRC)"));
  } else {
    Serial.print(F("RX: read error, code "));
    Serial.println(st);
  }

  // Re-arm for the next packet.
  radio.startReceive();
}
