# BusAnalizer CAN / CAN FD Traffic Generator

This project turns the generator board into a runtime-configurable CAN / CAN FD traffic source for BusAnalizer testing.

The generator uses:

- **FDCAN1** as the traffic transmitter.
- **FDCAN2** as an independent receiver / validator and ACK node.
- **USART1** as the runtime command console.
- An **80 MHz FDCAN kernel clock**.

The CAN configuration can be changed at runtime from the UART console. Rebuilding or reflashing the generator is not required when switching between the supported bitrates and CAN modes.

## Default configuration after reset

The generator boots in:

```text
CAN FD + BRS
Nominal bitrate : 1 Mbit/s
Data bitrate    : 5 Mbit/s
Payload         : 64 bytes
CAN ID          : 0x100, standard 11-bit ID
Stress          : 100% / maximum rate
```

The startup console should contain lines similar to:

```text
================================
 CAN / CAN FD traffic generator
================================
Boot OK
CPU          : 400 MHz
Default      : CAN FD+BRS 1 Mbit/s / 5 Mbit/s
Pattern      : ID=100 counter + deterministic pattern
CAN2         : independent RX validator / ACK node enabled
Type ? for runtime commands
```

## UART console

USART1 configuration:

```text
115200 baud
8 data bits
No parity
1 stop bit
No flow control
```

For example, on Linux:

```bash
minicom -D /dev/ttyUSB0 -b 115200
```

Use the actual `/dev/ttyUSBx` device assigned to the USB/UART adapter.

> **Important:** commands such as `can fd 1m 5m` are entered in the **generator UART terminal**, not in the Linux shell.

## Command summary

Type:

```text
?
```

to display the command list.

### Classic CAN

```text
can classic 500k
can classic 1m
```

### CAN FD + BRS

```text
can fd 500k 2m
can fd 500k 5m
can fd 1m 2m
can fd 1m 5m
```

The first bitrate is the **nominal/arbitration bitrate**. The second bitrate is the **CAN FD data bitrate**.

Examples:

```text
can fd 1m 5m
```

means:

```text
Nominal phase : 1 Mbit/s
Data phase    : 5 Mbit/s
CAN FD        : enabled
BRS           : enabled
Payload       : 64 bytes
```

and:

```text
can classic 500k
```

means:

```text
Classic CAN   : enabled
Bitrate       : 500 kbit/s
Payload       : 8 bytes
```

### Display current configuration

```text
can status
```

Example CAN FD output:

```text
CAN STATUS mode=FD+BRS nominal=1000000 data=5000000 payload=64 N[brp=5 sjw=2 seg1=13 seg2=2] D[brp=1 sjw=3 seg1=12 seg2=3]
STRESS set=100% mode=MAX
```

Example Classic CAN output:

```text
CAN STATUS mode=CLASSIC nominal=500000 payload=8 N[brp=10 sjw=2 seg1=13 seg2=2]
STRESS set=100% mode=MAX
```

## Supported timing profiles

All profiles use the 80 MHz FDCAN kernel clock.

| Mode | Nominal bitrate | Nominal timing | Data bitrate | Data timing |
| --- | ---: | --- | ---: | --- |
| Classic | 500 kbit/s | BRP=10, SJW=2, SEG1=13, SEG2=2 | - | - |
| Classic | 1 Mbit/s | BRP=5, SJW=2, SEG1=13, SEG2=2 | - | - |
| FD+BRS | 500 kbit/s | BRP=10, SJW=2, SEG1=13, SEG2=2 | 2 Mbit/s | BRP=2, SJW=4, SEG1=15, SEG2=4 |
| FD+BRS | 500 kbit/s | BRP=10, SJW=2, SEG1=13, SEG2=2 | 5 Mbit/s | BRP=1, SJW=3, SEG1=12, SEG2=3 |
| FD+BRS | 1 Mbit/s | BRP=5, SJW=2, SEG1=13, SEG2=2 | 2 Mbit/s | BRP=2, SJW=4, SEG1=15, SEG2=4 |
| FD+BRS | 1 Mbit/s | BRP=5, SJW=2, SEG1=13, SEG2=2 | 5 Mbit/s | BRP=1, SJW=3, SEG1=12, SEG2=3 |

Nominal sample point is 87.5% for the provided 500 kbit/s and 1 Mbit/s profiles.

Data sample points are:

```text
2 Mbit/s : 80%
5 Mbit/s : 81.25%
```

## Transmit Delay Compensation

FDCAN1 Transmit Delay Compensation (TDC) is enabled automatically when transmitting CAN FD+BRS frames.

The firmware calculates:

```text
TDC offset = DataPrescaler * DataTimeSeg1
```

Therefore:

```text
2 Mbit/s : 2 * 15 = 30
5 Mbit/s : 1 * 12 = 12
```

TDC is not enabled for Classic CAN.

## Generated CAN traffic

The generator continuously transmits standard ID:

```text
0x100
```

Frame format:

```text
Classic CAN : 8-byte data frame
CAN FD+BRS  : 64-byte data frame
```

Payload:

```text
bytes 0..3 : 32-bit little-endian monotonically increasing frame counter
bytes 4..N : (counter + byte_index) & 0xFF
```

Example start of an FD payload:

```text
5F 3C 06 00 63 64 65 66 ...
60 3C 06 00 64 65 66 67 ...
61 3C 06 00 65 66 67 68 ...
```

This deterministic pattern allows missing, duplicated, reordered, truncated, or corrupted frames to be detected easily.

## Stress control

The UART console also accepts a number from `0` to `100`.

```text
0
25
50
75
100
```

Meaning:

```text
0     pause CAN transmission
1-99  rate-limited traffic
100   maximum possible generator rate
```

`100` is normally used for BusAnalizer saturation and throughput tests.

To resume maximum traffic after pausing:

```text
100
```

## Runtime mode switching

The generator can switch modes without a reset.

Example:

```text
can fd 1m 5m
can status

can classic 500k
can status

can fd 500k 2m
can status
```

During a configuration change the firmware:

1. Stops FDCAN1 transmission.
2. Stops FDCAN2.
3. Reconfigures both FDCAN peripherals.
4. Reconfigures the FDCAN2 receive path.
5. Starts FDCAN2 first so the ACK/validation node is active.
6. Starts FDCAN1.
7. Enables TDC when required for FD+BRS.
8. Resets the generator validation/statistics counters.

## Internal validation

FDCAN2 independently receives the traffic produced by FDCAN1 and validates it.

A normal status report looks like:

```text
TXQ=690113 RX=690081 ADDERR=0 free=0 rxFill=0 | C1 TEC=0 REC=0 EP=0 BO=0 LEC=7 | C2 TEC=0 REC=0 EP=0 BO=0 LEC=7
GENX RUN tx=690113 rx=690081 d=32 next=690081 ok=690081 bad=0
SEQ first=0 last=690080 err=0 miss=0 dup=0 back=0 fut=0
RX id=0 dlc=0 hdr=0 esi=0 pf=0 pb=0 read=0 lost=0 max=5
```

`RUN` means the counters are clean but TX frames are still in flight. Enter
`0` to pause transmission and allow both hardware FIFOs to drain. A completely
clean, reconciled run then changes to `GENX PASS`. Any frame, FIFO, read, or
receive-side protocol error changes it to `GENX FAIL` and prints first/latest
failure snapshots.

### Important counters

| Counter | Meaning | Healthy value |
| --- | --- | ---: |
| `ADDERR` | Failed attempts to add TX frames to FDCAN1 | 0 |
| `TEC` | CAN transmit error counter | 0 |
| `REC` | CAN receive error counter | 0 |
| `EP` | Error passive state | 0 |
| `BO` | Bus-off state | 0 |
| `err` | Sequence discontinuities | 0 |
| `miss` | Missing counter values, including loss before the first RX frame | 0 |
| `dup` | Duplicate counter events | 0 |
| `back` | Backward/reordered counter events | 0 |
| `fut` | RX counter not yet queued by FDCAN1 | 0 |
| `id` / `dlc` | Unexpected CAN identifier/type or DLC | 0 |
| `hdr` / `esi` | Wrong Classic/FD/BRS or ESI state | 0 |
| `pf` / `pb` | Frames / individual bytes with payload mismatch | 0 |
| `read` | HAL RX read failures | 0 |
| `lost` | FDCAN2 RX FIFO message-lost events | 0 |
| `max` | Maximum observed FDCAN2 FIFO fill level | informational |

`LEC=7` can appear during normal operation; the important health indicators are the error counters and validation counters above.

A clean generator test should keep:

```text
ADDERR=0
TEC=0
REC=0
EP=0
BO=0
err=0
miss=0
dup=0
back=0
fut=0
id=0
dlc=0
hdr=0
esi=0
pf=0
pb=0
read=0
lost=0
```

## Connecting the generator for BusAnalizer testing

The generator uses its two CAN channels together so that FDCAN2 can ACK and independently validate traffic from FDCAN1.

The BusAnalizer sniffer is connected in parallel to the same CAN bus.

Conceptually:

```text
Generator FDCAN1 TX node -----+
                              +---- CAN_H / CAN_L bus
Generator FDCAN2 ACK/RX node -+
                              +---- BusAnalizer sniffer
```

Use correct CAN termination for the physical bus. The analyzer should remain in listen-only / bus-monitoring mode so it does not ACK or disturb traffic.

## Example BusAnalizer tests

### CAN FD 1 Mbit/s / 5 Mbit/s

Generator UART:

```text
can fd 1m 5m
100
can status
```

Linux / BusAnalizer:

```bash
sudo ip link set can0 down
sudo ip link set can0 type can \
    bitrate 1000000 \
    dbitrate 5000000 \
    fd on \
    listen-only on
sudo ip link set can0 up

ip -details -statistics link show can0
candump -x can0
```

Expected frames are ID `0x100`, 64 bytes, CAN FD+BRS.

### Classic CAN 500 kbit/s

Generator UART:

```text
can classic 500k
100
can status
```

Linux / BusAnalizer:

```bash
sudo ip link set can0 down
sudo ip link set can0 type can \
    bitrate 500000 \
    fd off \
    listen-only on
sudo ip link set can0 up

ip -details -statistics link show can0
candump can0
```

Expected frames are ID `0x100`, 8-byte Classic CAN frames.

## Full configuration test matrix

| Generator command | Analyzer configuration |
| --- | --- |
| `can classic 500k` | `bitrate 500000 fd off` |
| `can classic 1m` | `bitrate 1000000 fd off` |
| `can fd 500k 2m` | `bitrate 500000 dbitrate 2000000 fd on` |
| `can fd 500k 5m` | `bitrate 500000 dbitrate 5000000 fd on` |
| `can fd 1m 2m` | `bitrate 1000000 dbitrate 2000000 fd on` |
| `can fd 1m 5m` | `bitrate 1000000 dbitrate 5000000 fd on` |

For the BusAnalizer passive sniffer, add:

```text
listen-only on
```

to each SocketCAN configuration.

## Useful long capture

For a high-load analyzer test:

```text
# generator UART
can fd 1m 5m
100
```

Then on Linux:

```bash
candump -d -r 8388608 -n 1000000 -l can0
```

During the capture, watch the generator UART. Its internal validation should remain at zero errors.

After the capture:

```bash
ip -details -statistics link show can0
```

Check that the Linux RX error/drop counters are also zero.

## Known-good hardware validation

The runtime generator configuration has been tested successfully on hardware in all six supported configurations:

```text
Classic 500 kbit/s
Classic 1 Mbit/s
FD 500 kbit/s / 2 Mbit/s
FD 500 kbit/s / 5 Mbit/s
FD 1 Mbit/s / 2 Mbit/s
FD 1 Mbit/s / 5 Mbit/s
```

Runtime transitions between Classic CAN and CAN FD were also tested without reflashing or rebooting.

The 1 Mbit/s / 5 Mbit/s FD+BRS configuration has run for more than one million internally validated frames with the generator validation counters remaining at zero.

## Quick reference

```text
?                       show help
can status              show CAN configuration + stress state

can classic 500k        Classic CAN, 500 kbit/s, 8 bytes
can classic 1m          Classic CAN, 1 Mbit/s, 8 bytes

can fd 500k 2m          FD+BRS, 500k / 2M, 64 bytes
can fd 500k 5m          FD+BRS, 500k / 5M, 64 bytes
can fd 1m 2m            FD+BRS, 1M / 2M, 64 bytes
can fd 1m 5m            FD+BRS, 1M / 5M, 64 bytes

0                       pause traffic
1..99                   rate-limited traffic
100                     maximum traffic
```
