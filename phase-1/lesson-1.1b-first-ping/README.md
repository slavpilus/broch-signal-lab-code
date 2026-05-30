# Lesson 1.1b — Two boards on 868 MHz: the first ping (receiver)

Companion code for **Phase 1 · Lesson 1.1b** of the Broch Signal series.

**→ Read the post: [Two boards on 868 MHz: the Discovery kit's first ping](https://www.brochsignal.com/field-notes/two-boards-868-first-ping)**

Raw LoRa, **RX-only**. This firmware turns the diymore "LoRa V4" board into a
receiver and listens for the beacon the Discovery kit transmits in
[Lesson 1.1a](https://www.brochsignal.com/field-notes/platformio-and-the-discovery-kit).
For every packet it hears it logs the text, RSSI, and SNR — the first time the
two radios prove they are on the same air. The two-way ping-pong (the V4 talks
back, the Discovery kit listens) is the second half of the lesson.

## Hardware

- diymore **"LoRa V4"** board — a **Heltec WiFi LoRa 32 V4 clone**
  (ESP32-S3R2 + Semtech **SX1262**, EU868). Confirm the silkscreen reads
  **V4 / HTIT-WB32LAF**; a true V3 uses the same LoRa pins.
- A USB-C cable to the board (native USB — power, flashing, and serial).
- The Discovery kit from 1.1a, already flashed and transmitting.

## Build, flash, monitor

PlatformIO Core must be installed (`pip install platformio`, or the VS Code
extension).

```bash
pio run            # compile
pio run -t upload  # flash over native USB (hold BOOT if it won't enumerate)
pio device monitor # 115200 baud — the native USB-CDC port
```

Expected serial output, with the Discovery kit transmitting nearby:

```
Broch Signal — Lesson 1.1b — raw LoRa, RX-only (the V4 listens)
radio.begin()... ok
config: 868.1 MHz  SF7  BW125  CR4/5  sync 0x12  preamble 8  CRC off
listening for "BrochSignal 1.1a #N" ...

RX #1  "BrochSignal 1.1a #42"  19 bytes   RSSI -41.5 dBm   SNR 9.75 dB   f_err 1200 Hz
RX #2  "BrochSignal 1.1a #43"  19 bytes   RSSI -42.0 dBm   SNR 9.50 dB   f_err 1180 Hz
```

(The counter in the payload starts wherever the Discovery kit happens to be —
it has been transmitting since 1.1a.)

## Radio settings — these must equal Lesson 1.1a

| Parameter        | Value                | Why it matters                              |
|------------------|----------------------|---------------------------------------------|
| Frequency        | 868.1 MHz            | off-frequency = deaf                        |
| Spreading factor | SF7                  | SF mismatch = deaf                          |
| Bandwidth        | 125 kHz              | BW mismatch = deaf                          |
| Coding rate      | 4/5                  | header decode fails if it disagrees         |
| Sync word        | `0x12` (private)     | SX1262 `0x12`→`0x1424`, matches SX1276 `0x12` |
| Preamble length  | 8 symbols            | short/long preamble = missed sync           |
| **CRC**          | **off**              | 1.1a sends no CRC; RadioLib defaults it ON  |

CRC is the quiet one. RadioLib enables CRC by default; the 1.1a firmware never
calls `enableCrc()`, so its packets carry none. Leave CRC on here and the V4
rejects every packet as corrupt — a receiver that is working perfectly and
reports nothing. `radio.setCRC(false)` in [`src/main.cpp`](src/main.cpp) is the fix.

## Three SX1262 things this firmware handles explicitly

1. **TCXO on DIO3.** The reference is a TCXO, not a bare crystal, so a control
   voltage is supplied — `TCXO_VOLTAGE` (default 1.6 V). If `radio.begin()`
   returns a negative code on your unit, sweep it: 1.8 V, then 0.0 V (no TCXO).
   These clones genuinely vary; note the value that returns `0`.
2. **DIO2 as RF switch.** `setDio2AsRfSwitch(true)` — the antenna switch is
   driven by the radio's own DIO2 pin, not a separate GPIO.
3. **V4 RF front-end enable (GPIO2).** V4 clones add a GC1109 front-end whose
   enable line sits on GPIO2; it is driven HIGH so the receive path is live.
   Harmless on a true V3. If a confirmed-V4 still hears nothing, GPIO7 and
   GPIO46 are the documented belt-and-braces front-end lines to try next.

## Pin map (Heltec WiFi LoRa 32 V3/V4)

| SX1262 | GPIO |   | SPI  | GPIO |
|--------|------|---|------|------|
| NSS    | 8    |   | SCK  | 9    |
| DIO1   | 14   |   | MISO | 11   |
| RESET  | 12   |   | MOSI | 10   |
| BUSY   | 13   |   |      |      |

## Versions

Pinned in [`platformio.ini`](platformio.ini): `jgromes/RadioLib`. Bump
deliberately — radio libraries change defaults (CRC, sync word) between
versions, and a changed default is exactly what makes a receiver go silent.
