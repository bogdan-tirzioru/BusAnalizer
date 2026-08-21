ip link set can0 down 2>/dev/null
ip link set can1 down 2>/dev/null
ip link set can0 type can     bitrate 1000000 sample-point 0.875     dbitrate 5000000 dsample-point 0.8125     fd on listen-only on
ip link set can1 type can     bitrate 1000000 sample-point 0.875     dbitrate 5000000 dsample-point 0.8125     fd on listen-only on
ip link set can0 up
ip link set can1 up