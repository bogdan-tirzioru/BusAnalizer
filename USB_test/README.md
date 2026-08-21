# BusAnalizer USB_test — dual CAN / CAN FD SocketCAN analyzer

`USB_test` is the current BusAnalizer 1 analyzer firmware for the STM32H750 board.
It exposes both FDCAN channels to Linux through the standard `gs_usb` driver, so
Linux sees the board as two SocketCAN interfaces (`can0` and `can1`).

The analyzer is intentionally receive-only and should be used in listen-only mode.
No custom Linux driver is required.

## Current capabilities

- STM32H750, 480 MHz CPU
- FDCAN1 + FDCAN2
- 80 MHz FDCAN kernel clock
- Classic CAN receive
- CAN FD + BRS receive
- Two independent gs_usb channels
- Standard Linux `gs_usb` + SocketCAN
- 4096-frame SRAM capture queue shared by both channels
- USB Full-Speed device, 12 Mbit/s, 64-byte bulk packets
- gs_usb record size: 20 bytes Classic CAN, 76 bytes CAN FD
- Pipelined USB IN transfers with two TX buffers
- USB TX FIFO preload optimization
- Nonblocking USART1 diagnostic logger using DMA
- Detailed FDCAN / SRAM / USB performance counters

Hardware timestamps are not exposed to Linux yet.

## Build

Open `USB_test/USB_test.ioc` or the `USB_test` STM32CubeIDE project and build the
`Debug` configuration.

The repository also contains a Jenkins pipeline. The `BusAnalizer-build` job uses
the repository `Jenkinsfile` and accepts a `GIT_REF` parameter, for example:

```text
*/master
*/docs/usage-guides
```

The CI build performs a clean STM32CubeIDE headless build of `USB_test/Debug` and
archives the ELF and build information.

## Flash and connect

1. Flash `USB_test` to the BusAnalizer 1 board.
2. Connect the board USB device port to the Linux PC.
3. Connect CAN1 and/or CAN2 to the buses to be monitored.
4. Make sure the CAN buses have correct physical termination.
5. The analyzer must stay passive; Linux configuration uses `listen-only on`.

Load the Linux driver and confirm enumeration:

```bash
sudo modprobe gs_usb
ip -brief link | grep can
ip -details link show can0
ip -details link show can1
```

After enumeration, the firmware USART1 log should report:

```text
gs_usb configured; CAN channels 0/1 available
```

## Quick start: CAN FD 1 Mbit/s / 5 Mbit/s

The repository provides `scripts/start_can.sh` for the configuration used in the
current stress tests.

From the repository root:

```bash
sudo bash scripts/start_can.sh
```

Equivalent commands are:

```bash
sudo ip link set can0 down 2>/dev/null
sudo ip link set can1 down 2>/dev/null

sudo ip link set can0 type can \
    bitrate 1000000 sample-point 0.875 \
    dbitrate 5000000 dsample-point 0.8125 \
    fd on listen-only on

sudo ip link set can1 type can \
    bitrate 1000000 sample-point 0.875 \
    dbitrate 5000000 dsample-point 0.8125 \
    fd on listen-only on

sudo ip link set can0 up
sudo ip link set can1 up
```

Check both interfaces:

```bash
ip -details -statistics link show can0
ip -details -statistics link show can1
```

View traffic in two terminals:

```bash
candump -x can0
```

```bash
candump -x can1
```

## Classic CAN examples

### 500 kbit/s

```bash
sudo ip link set can0 down
sudo ip link set can0 type can bitrate 500000 sample-point 0.875 fd off listen-only on
sudo ip link set can0 up
candump can0
```

### 1 Mbit/s

```bash
sudo ip link set can0 down
sudo ip link set can0 type can bitrate 1000000 sample-point 0.875 fd off listen-only on
sudo ip link set can0 up
candump can0
```

Repeat the same commands with `can1` for the second analyzer channel.

## Long capture / loss test

A useful one-channel endurance command is:

```bash
candump -d -r 8388608 -n 1000000 -l can0
```

After the run:

```bash
ip -details -statistics link show can0
```

For dual-channel validation, run one capture per interface in separate terminals
or processes so each channel can be checked independently.

Known Classic CAN reference result from 2026-08-19:

```text
Frames checked : 1000000
Sequence gaps  : 0
Missing frames : 0
Duration       : 115.784 seconds
Average rate   : 8636.7 frames/s
USB payload    : 172.7 kB/s
```

## USART1 diagnostics

USART1 is used for analyzer status and performance diagnostics.

Typical terminal setup:

```bash
minicom -D /dev/ttyUSB0 -b 115200
```

Use the actual serial device assigned to the USB/UART adapter.

The firmware reports a line similar to:

```text
CAN1 2150 fps buf=0 hi=2 drop=0 lost=0 max=1 | CAN2 2150 fps lost=0 max=1 | USB tx=.../... bytes=... idle=... gap=...us pre=.../... logdrop=0
```

Meaning:

| Field | Meaning | Healthy behavior |
| --- | --- | --- |
| `CAN1/CAN2 fps` | received frames during the last interval | matches generator rate |
| `buf` | current shared SRAM queue depth | low / bounded |
| `hi` | queue high-water mark in the interval | bounded, not continuously rising |
| `drop` | software capture queue drops | `0` |
| `lost` | FDCAN RX FIFO message-lost events | `0` |
| `max` | maximum FDCAN FIFO fill level | low; `1` is excellent |
| `USB tx=A/B` | USB transfers started/completed | closely tracks input traffic |
| `bytes` | completed gs_usb payload bytes | informational throughput metric |
| `idle` | times the USB IN endpoint became idle | lower is better under sustained load |
| `gap` | maximum USB completion-to-completion gap | useful for locating USB stalls |
| `pre=H/F` | TX FIFO preload hits/fallbacks | high hit rate preferred |
| `logdrop` | diagnostic UART messages dropped | `0` |

## Pass criteria for a throughput test

A clean analyzer run should satisfy all of the following:

```text
capture drop = 0
CAN1 fifo lost = 0
CAN2 fifo lost = 0
logger drop = 0
Linux RX drops/errors = 0
```

The SRAM high-water mark should remain bounded. If `hi` keeps increasing while
FDCAN `max` stays low, the input side is healthy and the bottleneck is farther
downstream in the USB/host path.

If the USB endpoint is continuously busy (`idle` near zero) while the SRAM queue
continues to grow, the practical USB streaming limit has been reached.

## Current CAN FD characterization

The current stress configuration is:

```text
CAN FD + BRS
Nominal bitrate : 1 Mbit/s
Data bitrate    : 5 Mbit/s
Payload         : 64 bytes
```

A confirmed dual-channel point is 75% generator stress, corresponding to about:

```text
3225 frames/s per channel
6450 frames/s total
490.2 kB/s gs_usb payload (76 bytes per FD host record)
```

This is a characterization point, not a final product limit. The current work is
to sweep higher loads while watching the new FDCAN/SRAM/USB statistics.

## Stop capture

```bash
sudo ip link set can0 down
sudo ip link set can1 down
```

## Related files

- `scripts/start_can.sh` — known-good dual-channel 1M/5M SocketCAN setup
- `Core/Src/can_sniffer.c` — FDCAN receive path
- `Core/Src/can_capture_buffer.c` — shared SRAM queue
- `Core/Src/gs_usb_app.c` — CAN-to-gs_usb streaming
- `USB_DEVICE/App/usbd_bulk.c` — gs_usb USB class and transfer pipeline
- `USB_DEVICE/App/gs_usb_stats.c` — USB performance instrumentation
- `generator_CAN/README.md` — detailed generator reference
- `generator_CAN/USAGE.md` — quick generator test workflow
