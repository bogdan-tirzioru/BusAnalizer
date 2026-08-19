# USB_test gs_usb CAN1 sniffer

This branch exposes the existing STM32H750 CAN1 capture path directly to the
mainline Linux `gs_usb` driver. Linux creates a SocketCAN network interface, so
the custom `bulk_test` program and libusb permissions rule are not used here.

The firmware is intentionally RX-only. It advertises Classic CAN and
listen-only support, accepts Linux bit-timing configuration, and refuses a
`GS_CAN_MODE_START` request unless the listen-only flag is set.

## Firmware and USB identity

- FDCAN1 RX/TX: PA11/PA12, AF9
- CAN1 transceiver standby: PA3, low enables the transceiver
- USB bulk endpoints: OUT `0x01`, IN `0x81`
- compatible gs_usb identity: `1209:2323`
- one CAN channel (`can0`)
- 8 MHz FDCAN kernel clock
- Classic CAN only
- 4096 records / 64 KiB local SRAM capture ring

The VID/PID is the compatibility identity recognized by the upstream Linux
`gs_usb` driver. This is experimental firmware for the BusAnalizer hardware;
do not use this identity as a product allocation.

## Linux setup at 500 kbit/s

Install `can-utils`, flash the firmware, and reconnect USB:

```bash
sudo modprobe gs_usb
lsusb -d 1209:2323
dmesg | tail -30
ip -details link show can0
```

Bring CAN1 up as a passive 500 kbit/s sniffer:

```bash
sudo ip link set can0 down
sudo ip link set can0 type can bitrate 500000 sample-point 0.875 listen-only on
sudo ip link set can0 txqueuelen 1000
sudo ip link set can0 up
ip -details -statistics link show can0
candump -L can0
```

For 250 kbit/s, replace `500000` with `250000`.

Stop capture with:

```bash
sudo ip link set can0 down
```

## Loss check

During a stress test, inspect both SocketCAN and the USART1 diagnostic output:

```bash
watch -n 1 'ip -details -statistics link show can0'
```

The firmware prints `buffered`, `dropped`, and `fifo_lost` once per second on
USART1. A clean run requires `dropped=0` and `fifo_lost=0`. The ring is drained
only while `can0` is up; taking the interface down also stops capture and clears
the ring.

## Current scope

- Receive path only; no CAN transmission or echo frames
- Listen-only mode is mandatory
- No CAN FD
- No gs_usb hardware timestamps yet
- One `gs_host_frame` (20 bytes) is sent per USB transfer

This small first step deliberately proves the existing acquisition path through
SocketCAN before adding transmit support, CAN error frames, timestamps, or CAN
FD.
