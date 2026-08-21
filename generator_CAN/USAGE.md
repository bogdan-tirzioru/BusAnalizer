# generator_CAN — usage guide

`generator_CAN` turns a BusAnalizer 1 board into a repeatable Classic CAN / CAN FD
traffic source for analyzer stress testing.

For the complete timing tables and implementation details, also see
[`README.md`](README.md). This file is the quick operational workflow used during
BusAnalizer testing.

## Generator architecture

The firmware uses:

- **FDCAN1** — traffic transmitter
- **FDCAN2** — independent receiver / validator and ACK node
- **USART1** — runtime command console at 115200 baud, 8N1
- **80 MHz** FDCAN kernel clock

FDCAN1 and FDCAN2 must be connected to the same CAN bus for the normal generator
self-validation setup.

## Default after reset

```text
Mode            : CAN FD + BRS
Nominal bitrate : 1 Mbit/s
Data bitrate    : 5 Mbit/s
Payload         : 64 bytes
CAN ID          : 0x100 standard
Stress          : 100% maximum possible rate
```

Payload bytes 0..3 contain a little-endian monotonically increasing frame counter.
The rest of the payload is deterministic, allowing missing or corrupted frames to
be detected.

## Open the UART console

Example on Linux:

```bash
minicom -D /dev/ttyUSB0 -b 115200
```

Commands below are entered in the generator UART terminal, not in the Linux shell.

Type:

```text
?
```

for the built-in help.

## CAN mode commands

Classic CAN:

```text
can classic 500k
can classic 1m
```

CAN FD + BRS:

```text
can fd 500k 2m
can fd 500k 5m
can fd 1m 2m
can fd 1m 5m
```

Show the active configuration:

```text
can status
```

## Stress control

Enter a number from `0` to `100`:

```text
0
25
50
75
90
100
```

Behavior:

```text
0      pause traffic
1..99  rate-limited traffic
100    maximum possible generator rate; limiter bypassed
```

For 1..99%, the current reference is 4300 frames/s:

```text
target_fps ≈ 4300 × stress / 100
```

Useful points:

| Stress | Approx. frames/s per generator |
| ---: | ---: |
| 25% | 1075 |
| 50% | 2150 |
| 60% | 2580 |
| 70% | 3010 |
| 75% | 3225 |
| 80% | 3440 |
| 90% | 3870 |
| 99% | 4257 |

**Important:** `100%` is not a fixed 4300 frames/s. At 100% the rate limiter is
bypassed and the firmware transmits as fast as the generator can sustain.

## Single analyzer channel wiring

Use both generator FDCAN channels on one physical bus:

```text
Generator FDCAN1 TX  ----+
                          +---- CAN_H / CAN_L ---- BusAnalizer CAN1 or CAN2
Generator FDCAN2 ACK -----+
```

The analyzer is passive/listen-only and does not provide ACK. FDCAN2 on the
generator provides ACK and independently validates the frames sent by FDCAN1.

Use correct CAN termination at the physical ends of the bus.

## Dual analyzer channel stress test

To drive **two independent BusAnalizer channels simultaneously**, use two generator
boards (or equivalent independent CAN traffic sources):

```text
Generator A FDCAN1 + FDCAN2 ---- Bus A ---- BusAnalizer CAN1
Generator B FDCAN1 + FDCAN2 ---- Bus B ---- BusAnalizer CAN2
```

The two buses must remain electrically independent.

For the current 1M/5M dual-FD test, enter on both generator consoles:

```text
can fd 1m 5m
75
can status
```

At 75% this gives approximately:

```text
Generator A : 3225 frames/s
Generator B : 3225 frames/s
Total input : 6450 frames/s
```

With a 64-byte CAN FD payload this is 412.8 kB/s of CAN application payload.
With the standard 76-byte gs_usb FD record, the analyzer transports about
490.2 kB/s of gs_usb payload.

## Linux analyzer setup for 1M/5M

On the BusAnalizer host PC, from the repository root:

```bash
sudo bash scripts/start_can.sh
```

Then verify:

```bash
ip -details -statistics link show can0
ip -details -statistics link show can1
```

View traffic:

```bash
candump -x can0
```

```bash
candump -x can1
```

## Generator self-check

During a stress test, the generator periodically reports status similar to:

```text
TXQ=... RX=... ADDERR=0 ... C1 TEC=0 REC=0 EP=0 BO=0 ... C2 TEC=0 REC=0 EP=0 BO=0 ...
GENCHK ... seqErr=0 gap=0 back=0 idErr=0 dlcErr=0 hdrErr=0 payErr=0 fifoLost=0 maxFIFO=...
```

A healthy generator should keep these at zero:

```text
ADDERR
TEC
REC
EP
BO
seqErr
gap
back
idErr
dlcErr
hdrErr
payErr
fifoLost
```

`maxFIFO` is informational. `LEC=7` can appear during normal operation; use the
actual error and validation counters above to judge generator health.

## Recommended throughput sweep

For finding the analyzer limit, keep the CAN configuration fixed and change only
the stress percentage on both generators:

```text
50%
60%
70%
75%
80%
90%
100%
```

At each point:

1. Let the rate stabilize.
2. Confirm both generator validation counters stay at zero.
3. Record the BusAnalizer USART diagnostics.
4. Check Linux interface RX/drop statistics.
5. Increase to the next stress point only after the current point is clean.

The analyzer diagnostic fields `drop`, `lost`, `hi`, USB `idle`, USB `gap`, and
`pre` are especially useful for identifying where saturation begins.

## Long capture example

Generator UART:

```text
can fd 1m 5m
75
```

Linux, one channel:

```bash
candump -d -r 8388608 -n 1000000 -l can0
```

Then inspect:

```bash
ip -details -statistics link show can0
```

For dual-channel qualification, run the same validation independently for `can1`.

## Quick command reference

```text
?                       show generator help
can status              show active CAN configuration and stress state

can classic 500k        Classic CAN, 500 kbit/s, 8-byte payload
can classic 1m          Classic CAN, 1 Mbit/s, 8-byte payload

can fd 500k 2m          FD+BRS, 500k / 2M, 64-byte payload
can fd 500k 5m          FD+BRS, 500k / 5M, 64-byte payload
can fd 1m 2m            FD+BRS, 1M / 2M, 64-byte payload
can fd 1m 5m            FD+BRS, 1M / 5M, 64-byte payload

0                       pause
1..99                   rate-limited stress
100                     maximum possible rate
```
