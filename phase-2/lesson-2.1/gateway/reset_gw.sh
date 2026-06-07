#!/bin/sh
# Concentrator reset for the Waveshare SX1303 868M LoRaWAN Gateway HAT (B)
# on a Raspberry Pi 4 running Raspberry Pi OS Lite 64-bit (Debian Trixie).
#
# Why this file exists: the SX130x has to be pulsed on its reset line before
# Basic Station opens the radio, and Basic Station does NOT do it for you.
# Every script you'll find online (including upstream reset_lgw.sh) drives the
# pin through /sys/class/gpio, which is DEAD on Trixie's kernel. This uses
# libgpiod's gpioset instead, in the v2 command syntax (libgpiod >= 2.0).
#
# Check your gpioset version: `gpioset --version`. This is written for v2.x
# (Trixie ships v2.2.1). For libgpiod v1, the flags differ.
#
# Pins are for the Waveshare "(B)" board. The generic Semtech reference uses
# RESET=23; this board uses 17. If the concentrator won't start (chip version
# reads as garbage, "failed to setup radio 0"), try RESET=23.

CHIP=gpiochip0
RESET=17          # SX130x reset      (Waveshare (B); Semtech reference uses 23)
POWER_EN=18       # LDO power enable

G="gpioset --hold-period 100ms -t0 -c $CHIP"

echo "power-enable GPIO$POWER_EN"
$G $POWER_EN=1

echo "reset SX130x via GPIO$RESET"
$G $RESET=1
$G $RESET=0
