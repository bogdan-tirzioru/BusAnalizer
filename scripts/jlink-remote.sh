#!/usr/bin/env bash
set -u

JLINK_SERVER="$HOME/.eclipse/com.st.stm32cube.ide.mcu.rcp.product_1.11.0_139060369_linux_gtk_x86_64/plugins/com.st.stm32cube.ide.mcu.externaltools.jlink.linux64_2.5.0.202506031126/tools/bin/JLinkRemoteServerCLExe"
JLINK_SERIAL=""269304468
PORT="19040"
LOG_FILE="$HOME/jlink-remote.log"

is_running() {
    ss -ltn 2>/dev/null | grep -qE "LISTEN[[:space:]].*:${PORT}[[:space:]]"
}

start_server() {
    if is_running; then
        echo "J-Link Remote Server is already running on port ${PORT}."
        exit 0
    fi

    if [[ ! -x "$JLINK_SERVER" ]]; then
        echo "Error: executable not found or not executable:"
        echo "  $JLINK_SERVER"
        exit 1
    fi

    echo "Starting J-Link Remote Server..."
    echo "Probe serial: ${JLINK_SERIAL}"
    echo "Port:         ${PORT}"
    echo "Log:          ${LOG_FILE}"

    nohup "$JLINK_SERVER" \
        -USB "$JLINK_SERIAL" \
        -Port "$PORT" \
        >>"$LOG_FILE" 2>&1 </dev/null &

    sleep 2

    if is_running; then
        echo "J-Link Remote Server started successfully."
        ss -ltnp 2>/dev/null | grep ":${PORT}" || true
    else
        echo "Error: the server did not start."
        echo "Last log lines:"
        tail -n 30 "$LOG_FILE" 2>/dev/null || true
        exit 1
    fi
}

stop_server() {
    if ! is_running; then
        echo "J-Link Remote Server is not running."
        exit 0
    fi

    echo "Stopping J-Link Remote Server..."
    pkill -f 'JLinkRemoteServerCLExe' 2>/dev/null || true
    sleep 2

    if is_running; then
        echo "Error: port ${PORT} is still in use."
        ss -ltnp 2>/dev/null | grep ":${PORT}" || true
        exit 1
    fi

    echo "J-Link Remote Server stopped."
}

show_status() {
    if is_running; then
        echo "J-Link Remote Server is running."
        ss -ltnp 2>/dev/null | grep ":${PORT}" || true
    else
        echo "J-Link Remote Server is not running."
        exit 1
    fi
}

show_log() {
    touch "$LOG_FILE"
    tail -f "$LOG_FILE"
}

case "${1:-start}" in
    start)   start_server ;;
    stop)    stop_server ;;
    restart) stop_server; start_server ;;
    status)  show_status ;;
    log)     show_log ;;
    *)
        echo "Usage: $0 {start|stop|restart|status|log}"
        exit 2
        ;;
esac
