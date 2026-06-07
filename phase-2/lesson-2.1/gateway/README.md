# Lesson 2.1 — Standing up my own LoRaWAN gateway (Basic Station → TTN)

Companion code for **Phase 2 · Lesson 2.1** of the Broch Signal series.

**→ Read the post: [From a desk to The Things Network: standing up my own gateway](https://www.brochsignal.com/field-notes/standing-up-my-own-gateway)**

There's no community gateway within ~30 km of Corby, so the only way onto The
Things Network was to run a gateway myself. This is the **full from-scratch
walkthrough** for a Raspberry Pi 4 + Waveshare SX1303 HAT, on a current (2026)
Raspberry Pi OS where most published gateway tutorials no longer work. The device
that joins *through* this gateway is Lesson 2.1b.

Tested on: Raspberry Pi 4 Model B, Waveshare SX1303 868M LoRaWAN Gateway HAT (B),
Raspberry Pi OS Lite 64-bit (Debian **Trixie**, libgpiod **2.2.1**), Basic Station
**2.0.6**, EU868.

---

## Hardware

- **Waveshare SX1303 868M LoRaWAN Gateway HAT (B)** — SX1303 8-channel concentrator
  (mini-PCIe module) + SX1250 front end + L76K GPS, EU868.
- **Raspberry Pi 4** — the Waveshare wiki targets the Pi 4, not the Pi 5 (the Pi 5
  needs GPIO changes). A dedicated gateway Pi also leaves a Pi 5 free for a network
  server later.
- Antenna on the **LoRa** SMA (the one *away* from the L76K GPS module), micro-SD,
  USB-C power, Ethernet.

The files in this folder (`reset_gw.sh`, `station.conf`, `tc.uri`,
`tc.key.example`, `basicstation.service`) are referenced by step number below.

---

## Step 1 — Flash the OS

Use Raspberry Pi Imager. Choose **Raspberry Pi OS Lite (64-bit)** (it's under
"Raspberry Pi OS (other)" — the headless Lite image, not the Desktop one).

In the Imager **customisation** (gear icon) before writing:
- Set a hostname (e.g. `brochsignal-gw`).
- Enable SSH, public-key auth.
- Set username + password.
- Set Wi-Fi (a backup; Ethernet is primary) and locale.

Flash, boot the Pi, give it ~2 minutes on first boot.

## Step 2 — First login and update

macOS/Linux resolve the Pi by mDNS, so the `.local` suffix is required:

```sh
ssh <user>@brochsignal-gw.local
sudo apt update && sudo apt full-upgrade -y
```

## Step 3 — Install the build prerequisites

`libgpiod` is the important one — it's how we reset the concentrator on Trixie.

```sh
sudo apt install -y git build-essential gpiod libgpiod-dev i2c-tools
gpioset --version       # confirm libgpiod 2.x (this guide assumes v2 syntax)
```

## Step 4 — Enable SPI, I2C, and the serial hardware

```sh
sudo raspi-config
```

- **Interface Options → SPI → Enable** (the data path to the concentrator).
- **Interface Options → I2C → Enable** (the HAT's temperature sensor sits here;
  Basic Station refuses to start if it can't read it).
- **Interface Options → Serial Port** → "login shell over serial?" **No** →
  "serial port hardware enabled?" **Yes** (frees the UART for the HAT GPS).

Say **No** to rebooting now; we fold the reboot into mounting the HAT.

(Non-interactive equivalents for SPI and I2C: `sudo raspi-config nonint do_spi 0`
and `sudo raspi-config nonint do_i2c 0`.)

## Step 5 — Mount the HAT and reboot

```sh
sudo shutdown -h now
```

Power off fully, then seat the HAT on the 40-pin header (press it down along its
whole length — a half-seated HAT causes intermittent SPI, see Gotchas). Screw the
antenna to the **LoRa** SMA. Power back up and `ssh` in again.

Verify the interfaces came up:

```sh
ls -l /dev/spidev*      # expect /dev/spidev0.0 and /dev/spidev0.1
gpiodetect              # expect gpiochip0
```

## Step 6 — Build Basic Station (with the 64-bit fix)

The `corecell` build only knows the 32-bit toolchain name, so on 64-bit Pi OS it
fails with `NO-TOOLCHAIN-FOUND-gcc`. The `sed` line points it at the Pi's native
`aarch64` compiler:

```sh
cd ~
git clone https://github.com/lorabasics/basicstation.git
cd basicstation
sed -i 's#^ARCH.corecell.*#ARCH.corecell  = aarch64-linux-gnu#' setup.gmk
make platform=corecell variant=std
~/basicstation/build-corecell-std/bin/station --version   # expect 2.0.x (corecell/std)
```

## Step 7 — Install the concentrator reset script

Get this folder's files onto the Pi (clone your companion repo, or `scp` them).
The reset script must sit next to the `station` binary's home; we use
`~/basicstation/`:

```sh
cp reset_gw.sh ~/basicstation/reset_gw.sh
chmod +x ~/basicstation/reset_gw.sh
sh ~/basicstation/reset_gw.sh        # smoke test: prints the two echo lines, exits 0
```

`reset_gw.sh` drives the reset via libgpiod's `gpioset` (v2 syntax), because the
old `/sys/class/gpio` interface every tutorial uses is dead on Trixie. Reset is on
**GPIO17** for this "(B)" board (the Semtech reference uses 23).

## Step 8 — Create the config directory

```sh
mkdir -p ~/ttn-gw
cp station.conf tc.uri ~/ttn-gw/
curl -o ~/ttn-gw/tc.trust https://letsencrypt.org/certs/isrgrootx1.pem.txt
```

- `station.conf` — describes the radio hardware (two SX1250s, SPI). The channel
  plan comes *down* from TTN.
- `tc.uri` — TTN's LNS address (EU1 cluster, Basic Station port 8887).
- `tc.trust` — the public ISRG Root X1 CA cert (fetched above; not in this repo).

## Step 9 — Register the gateway on TTN

Get the gateway EUI (Basic Station derives it from the Ethernet MAC):

```sh
awk -F: '{print toupper($1$2$3"FFFE"$4$5$6)}' /sys/class/net/eth0/address
```

In the TTN console (EU1 cluster, https://eu1.cloud.thethings.network):
1. **Gateways → Register gateway.** Enter that **EUI**, a lowercase **Gateway ID**
   (avoid baking in a location — IDs are immutable), frequency plan
   **Europe 863-870 MHz (SF9 for RX2)**. Create.
2. On the gateway page: **API keys → Add API key**, grant
   **"Link as Gateway to a Gateway Server for traffic exchange"**. Copy the key
   (starts `NNSXS.`) — shown once.

## Step 10 — Write the API key file (CRLF!)

The key file **must end with a CRLF** or Basic Station rejects it as a "malformed
auth token". A plain editor won't do that; use `printf`:

```sh
printf 'Authorization: Bearer %s\r\n' 'NNSXS.your-ttn-gateway-api-key' > ~/ttn-gw/tc.key
tail -c2 ~/ttn-gw/tc.key | od -An -tx1     # must print: 0d 0a
```

## Step 11 — Test in the foreground

```sh
cd ~/ttn-gw
~/basicstation/build-corecell-std/bin/station \
  --radio-init=$HOME/basicstation/reset_gw.sh --home=$HOME/ttn-gw
```

A clean start looks like:

```
chip version is 0x12 (v1.2)
found temperature sensor on port 0x39
Concentrator started (2s360ms)
Configuring for region: EU868 -- 863.0MHz..870.0MHz
```

`Configuring for region: EU868` only appears *after* TTN accepts the connection
and pushes the channel plan down, so that line is your proof you're on the network.
The TTN console should show **Connected**. `Ctrl-C` to stop.

## Step 12 — Run it as a service

So exactly one instance runs and it survives reboot:

```sh
sudo cp basicstation.service /etc/systemd/system/
sudo nano /etc/systemd/system/basicstation.service   # set User= and the three paths
sudo systemctl daemon-reload
sudo systemctl enable --now basicstation
journalctl -u basicstation -f
```

The console stays green. Done — gateway live on TTN.

---

## Gotchas that cost me time

- **`chip version is 0x05` / `failed to setup radio 0`** — not software. A garbage
  chip-version read is intermittent SPI: a marginally-seated HAT. Power down,
  reseat firmly, power-cycle. (Mine was lifted by a stick-on heatsink fouling the
  battery-slot corner.)
- **`Radio device '/dev/spidev0.0' in use by process: NNNN`** — only one process
  can own the concentrator. Don't hand-launch alongside the service. Clear strays
  with `pkill -9 -f bin/station`, then start the one systemd unit.
- **`SSL - peer ... connection going to be closed`** at startup is the normal
  two-step discovery→traffic handshake, not a failure.
- **`Beaconing suspend - missing GPS data`** is expected with no GPS fix indoors.
- **`Ignoring unknown field: antenna_gain`** is harmless; it's already removed from
  the `station.conf` in this repo.

## Region

EU868 throughout (`tc.uri` → EU1, `station.conf` radios at 867.5 / 868.5 MHz). The
channel plan comes down from TTN, so outside Europe just register the gateway with
your region's frequency plan and the LNS pushes the right channels.
