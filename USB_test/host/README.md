# CAN1 passive sniffer over USB bulk

This firmware turns the STM32H750 `USB_test` target into a passive Classic CAN
sniffer. CAN1 is fixed to **500 kbit/s**, listen-only mode and an 87.5% sample
point. Captured frames are buffered in a 4,096-entry (64 KiB) SRAM ring and sent
to Linux as BusAnalyzerII protocol v0.1 `CAN_DATA` messages over the existing
USB vendor bulk endpoints.

## Hardware profile

| Signal | STM32 pin | Configuration |
| --- | --- | --- |
| FDCAN1 RX | PA11 | AF9 |
| FDCAN1 TX | PA12 | AF9; recessive in bus-monitoring mode |
| CAN1 transceiver standby | PA3 | driven low to enable the transceiver |
| USB bulk OUT / IN | 0x01 / 0x81 | Full Speed, 64-byte max packet |

The FDCAN kernel clock is the 8 MHz HSE. Nominal timing is prescaler 1,
segment 1 = 13, segment 2 = 2 and SJW = 1. The controller does not transmit
ACKs or data while in `FDCAN_MODE_BUS_MONITORING`.

Connect CAN_H, CAN_L and a common ground. Terminate the bus at its two physical
ends; do not add a third termination at the analyzer unless it is an endpoint.

## Linux build

Install the libusb development package and build the host utility:

```sh
sudo apt install build-essential pkg-config libusb-1.0-0-dev
cd USB_test/host
make
```

To run without `sudo`, install the included udev rule:

```sh
sudo install -m 0644 99-usb-test.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger
```

Unplug and reconnect the board after installing the rule.

## Commands

```sh
./bulk_test info
./bulk_test status
./bulk_test config
./bulk_test config set500k
./bulk_test capture start
./bulk_test capture stop
./bulk_test capture clear
./bulk_test capture status
./bulk_test sniff
./bulk_test sniff 30
```

`sniff` prints candump-like records and runs until Ctrl-C. A duration in
seconds may be supplied. The firmware starts capture at boot, while the capture
commands allow acquisition to be stopped, resumed or cleared independently of
USB enumeration.

A healthy status under load has both `SRAM dropped` and `FDCAN FIFO lost`
at zero. The ring uses a drop-new policy when the host cannot keep up, preserving
the frames already captured and exposing the loss counter.

## BAII wire format

Every USB message begins with the 20-byte little-endian BAII v0.1 header:
`BAII`, version, message type, flags, transaction ID, sequence and payload
length. Control messages use command/response types. CAN traffic uses message
type `0x10`; its payload contains packed records:

| Field | Size |
| --- | ---: |
| Timestamp in microseconds | 8 |
| CAN identifier | 4 |
| Flags | 2 |
| Channel | 1 |
| Raw DLC | 1 |
| Data length | 1 |
| Reserved | 1 |
| Data | 0–8 in this Classic CAN profile |

The capture path accepts standard, extended and RTR frames, although the active
profile is intentionally locked to passive 500 kbit/s Classic CAN. The 16-bit
hardware timestamp is extended in capture order. After an idle gap longer than
one hardware timestamp wrap (65.536 ms), only modulo-wrap timing is available
until a future firmware revision adds a wider capture-side epoch.
