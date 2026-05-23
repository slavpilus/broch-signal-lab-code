/*
 * Broch Signal — Phase 1 · Lesson 1.1a
 * "Getting PlatformIO talking to the Discovery kit"
 *
 * Raw LoRa, TX-only. No LoRaWAN, no receiver yet — this firmware exists
 * only to prove the SX1276 inside the Murata module is keying the air.
 *
 *   Board : STM32 B-L072Z-LRWAN1 Discovery kit
 *           (Murata CMWX1ZZABZ = SX1276 + STM32L072CZ, EU868)
 *   Radio : 868.1 MHz, BW 125 kHz, SF7, CR 4/5, 14 dBm via PA_BOOST
 *
 * arduino-LoRa is a generic SX127x library. It does not know about three
 * things that are specific to THIS board, and all three have to be done
 * by hand here — they are the whole reason a stock LoRa sketch fails on
 * the Discovery kit:
 *
 *   1. TCXO power. The 32 MHz reference is a TCXO, not a plain crystal.
 *      Its supply is gated by a GPIO (PA12) — power it before the radio.
 *
 *   2. TCXO clock mode. The SX1276 must be told to expect an external
 *      clipped-sine clock on XTA, not to drive a crystal. That is one
 *      bit in register 0x4B, written by hand after LoRa.begin().
 *
 *   3. Antenna RF switch. PA_BOOST only reaches the SMA connector when
 *      the TX_BOOST leg of the on-board RF switch is driven high.
 *
 * Pin map: STM32duino variant header (variant_B_L072Z_LRWAN1.h) and the
 * ST B-L072Z-LRWAN1 BSP.
 */

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

// ---- Radio SPI + control pins (inside the Murata module) -------------------
#define RADIO_NSS    PA15   // SPI chip-select
#define RADIO_DIO0   PB4    // TxDone / RxDone interrupt (unused for blocking TX)
#define RADIO_RESET  PC0
#define RADIO_MOSI   PA7
#define RADIO_MISO   PA6
#define RADIO_SCLK   PB3

// ---- Board-specific lines arduino-LoRa will NOT touch -----------------------
#define RADIO_TCXO_VCC      PA12   // TCXO supply enable
#define RADIO_ANT_RX        PA1    // antenna switch: receive
#define RADIO_ANT_TX_BOOST  PC1    // antenna switch: transmit via PA_BOOST
#define RADIO_ANT_TX_RFO    PC2    // antenna switch: transmit via RFO

// ---- LoRa radio settings ---------------------------------------------------
#define LORA_FREQ_HZ   868100000L  // 868.1 MHz
#define LORA_SF        7
#define LORA_BW_HZ     125000L
#define LORA_CR_DENOM  5           // coding rate 4/5
#define LORA_SYNCWORD  0x12        // private network (LoRaWAN uses 0x34)
#define LORA_TX_DBM    14          // via PA_BOOST

// SX1276 register 0x4B, bit 4 = "TcxoInputOn": 1 = external clipped-sine TCXO.
#define SX1276_REG_TCXO        0x4B
#define SX1276_TCXO_INPUT_ON   0x10

// SPI1 is hard-wired to the radio inside the module.
SPIClass radioSPI(RADIO_MOSI, RADIO_MISO, RADIO_SCLK);

uint32_t txCount = 0;

// One raw SX1276 register write over the same SPI bus arduino-LoRa uses.
// Address byte with bit 7 set = write.
void radioWriteRegister(uint8_t reg, uint8_t value) {
  radioSPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  digitalWrite(RADIO_NSS, LOW);
  radioSPI.transfer(reg | 0x80);
  radioSPI.transfer(value);
  digitalWrite(RADIO_NSS, HIGH);
  radioSPI.endTransaction();
}

// Fast blink forever — used when setup cannot continue.
void halt(const char *why) {
  Serial.print(F("HALT: "));
  Serial.println(why);
  while (true) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(120);
    digitalWrite(LED_BUILTIN, LOW);
    delay(120);
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  delay(2000);                       // give yourself time to open the monitor
  Serial.println();
  Serial.println(F("Broch Signal — Lesson 1.1a — raw LoRa, TX-only"));

  // 1. Power the TCXO before the radio ever needs a clock.
  pinMode(RADIO_TCXO_VCC, OUTPUT);
  digitalWrite(RADIO_TCXO_VCC, HIGH);
  delay(10);                         // let the TCXO settle

  // 2. Park the antenna switch on the PA_BOOST transmit path. TX-only, so
  //    this is static — no need to flip it back and forth.
  pinMode(RADIO_ANT_RX, OUTPUT);
  pinMode(RADIO_ANT_TX_BOOST, OUTPUT);
  pinMode(RADIO_ANT_TX_RFO, OUTPUT);
  digitalWrite(RADIO_ANT_RX, LOW);
  digitalWrite(RADIO_ANT_TX_RFO, LOW);
  digitalWrite(RADIO_ANT_TX_BOOST, HIGH);

  // 3. Bring up the SX1276 through arduino-LoRa on the radio's own SPI bus.
  LoRa.setSPI(radioSPI);
  LoRa.setPins(RADIO_NSS, RADIO_RESET, RADIO_DIO0);
  Serial.print(F("LoRa.begin()... "));
  if (!LoRa.begin(LORA_FREQ_HZ)) halt("LoRa.begin failed — check ST-LINK / SPI");
  Serial.println(F("ok"));

  // 4. Tell the SX1276 its clock is an external clipped-sine TCXO.
  //    LoRa.begin() resets the chip, so this must happen AFTER begin().
  radioWriteRegister(SX1276_REG_TCXO, SX1276_TCXO_INPUT_ON);

  // 5. Radio parameters.
  LoRa.setSpreadingFactor(LORA_SF);
  LoRa.setSignalBandwidth(LORA_BW_HZ);
  LoRa.setCodingRate4(LORA_CR_DENOM);
  LoRa.setSyncWord(LORA_SYNCWORD);
  LoRa.setTxPower(LORA_TX_DBM);       // defaults to the PA_BOOST pin

  Serial.println(F("config: 868.1 MHz  SF7  BW125  CR4/5  14 dBm  PA_BOOST"));
  Serial.println(F("transmitting one packet every 10 s..."));
  Serial.println();
}

void loop() {
  txCount++;
  char msg[24];
  snprintf(msg, sizeof(msg), "BrochSignal 1.1a #%lu", (unsigned long)txCount);

  digitalWrite(LED_BUILTIN, HIGH);
  unsigned long t0 = millis();
  LoRa.beginPacket();
  LoRa.print(msg);
  LoRa.endPacket();                  // blocking — returns when TX is done
  unsigned long dt = millis() - t0;
  digitalWrite(LED_BUILTIN, LOW);

  Serial.print(F("TX #"));
  Serial.print(txCount);
  Serial.print(F("  \""));
  Serial.print(msg);
  Serial.print(F("\"  "));
  Serial.print(strlen(msg));
  Serial.print(F(" bytes  in "));
  Serial.print(dt);
  Serial.println(F(" ms"));

  delay(10000);  // 10 s — comfortably inside the EU868 1% duty cycle
}
