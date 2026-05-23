# Lesson 1.1a — Getting PlatformIO talking to the Discovery kit

Companion code for **Phase 1 · Lesson 1.1a** of the Broch Signal series.

**→ Read the post: [Getting PlatformIO talking to the Discovery kit](https://www.brochsignal.com/field-notes/platformio-and-the-discovery-kit)**

Raw LoRa, **TX-only**. No LoRaWAN, no receiver. The firmware transmits one
small packet every 10 seconds and reports it on the serial port — just enough
to prove the SX1276 inside the Murata module is keying the air. Confirming
reception with a second board is Lesson 1.1b.

## Hardware

- STM32 **B-L072Z-LRWAN1** Discovery kit (Murata CMWX1ZZABZ = SX1276 +
  STM32L072CZ, EU868)
- A micro-USB cable to the onboard ST-LINK — that is the only connection needed
  for power, flashing, and serial.

## Build, flash, monitor

PlatformIO Core must be installed (`pip install platformio`, or the VS Code
extension).

```bash
pio run            # compile
pio run -t upload  # flash over the onboard ST-LINK
pio device monitor # 115200 baud — the ST-LINK virtual COM port
```

Expected serial output:

```
Broch Signal — Lesson 1.1a — raw LoRa, TX-only
LoRa.begin()... ok
config: 868.1 MHz  SF7  BW125  CR4/5  14 dBm  PA_BOOST
transmitting one packet every 10 s...

TX #1  "BrochSignal 1.1a #1"  19 bytes  in 62 ms
TX #2  "BrochSignal 1.1a #2"  19 bytes  in 62 ms
```

## Radio settings

| Parameter      | Value      |
|----------------|------------|
| Frequency      | 868.1 MHz  |
| Spreading factor | SF7      |
| Bandwidth      | 125 kHz    |
| Coding rate    | 4/5        |
| Sync word      | `0x12` (private) |
| TX power       | 14 dBm via PA_BOOST |

## Three board-specific things `arduino-LoRa` does not do for you

`arduino-LoRa` is a generic SX127x library. The B-L072Z-LRWAN1 needs three
extra steps, all handled explicitly in [`src/main.cpp`](src/main.cpp):

1. **TCXO power** — the 32 MHz reference is a TCXO, not a crystal. Its supply
   is gated by GPIO **PA12**; it must be driven high before the radio runs.
2. **TCXO clock mode** — the SX1276 must be told to expect an external
   clipped-sine clock. That is bit 4 of register `0x4B`, written by hand.
3. **Antenna RF switch** — PA_BOOST only reaches the SMA connector when the
   **PC1** (TX_BOOST) leg of the on-board switch is high.

Miss any one of them and the firmware runs, the serial log ticks up, and yet
nothing usable leaves the antenna.

## Versions

Pinned in [`platformio.ini`](platformio.ini): `ststm32@19.6.0`,
`sandeepmistry/LoRa@0.8.0`. Bump deliberately.
