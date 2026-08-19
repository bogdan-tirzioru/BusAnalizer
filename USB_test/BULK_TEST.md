# USB_test vendor bulk experiment

This branch replaces CDC ACM with a vendor-specific USB interface containing
two bulk endpoints:

| Direction | Endpoint | Full-speed packet |
|-----------|----------|-------------------|
| Host to STM32 | `0x01` OUT | 64 bytes |
| STM32 to host | `0x81` IN | 64 bytes |

The STM32 continuously submits 4096-byte blocks. Every IN block begins with
`BULK`, followed by a 32-bit little-endian sequence number and a deterministic
payload. OUT data is received in 4096-byte windows, consumed, and counted.
USART1 reports IN/OUT counters once per second.

The experimental USB identity is `0483:5741`. This keeps it distinct from the
CDC checkpoint `0483:5740`. It is for laboratory use, not a production USB
VID/PID assignment.

## Linux test

Install the build dependency:

```bash
sudo apt install build-essential pkg-config libusb-1.0-0-dev
```

Build and run a 10-second bidirectional test:

```bash
cd USB_test/host
make
sudo ./bulk_test 10
```

Before running the test, enumeration can be checked with:

```bash
lsusb -d 0483:5741
lsusb -t
sudo lsusb -d 0483:5741 -v
```

A `/dev/ttyACM0` node is not expected because this is no longer a CDC ACM
device. The application accesses interface 0 directly through `libusb`.

## Expected USART1 log

```text
USB_test starting
USB vendor bulk stack started; waiting for Linux
USB vendor bulk configured by Linux
BULK 1s: IN=... B OUT=... B, total IN=... OUT=...
```

## Backward command channel

Commands travel from Linux to STM32 as fixed 32-byte `BCMD` frames. Responses
are fixed 32-byte `BRSP` frames on bulk IN. A response is prioritized after
the current stream block completes.

| Command | ID | Purpose |
|---------|----|---------|
| PING | `0x01` | Echo up to 16 payload bytes |
| GET_INFO | `0x02` | Read protocol, speed, endpoints and block size |
| GET_STATS | `0x03` | Read cumulative IN/OUT counters |
| STREAM_CONTROL | `0x04` | Start or stop continuous IN streaming |

Linux examples:

```bash
sudo ./bulk_test ping
sudo ./bulk_test info
sudo ./bulk_test stats
sudo ./bulk_test stream stop
sudo ./bulk_test stream start
sudo ./bulk_test 10
```

The normal numeric benchmark automatically enables streaming before measuring.

## CubeMX warning

CubeMX still describes the generated CDC starting point. Regenerating code can
restore CDC registration and descriptors. Keep this experimental branch as a
manual custom-class implementation until the bulk design is finalized.
