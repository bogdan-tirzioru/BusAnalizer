#!/usr/bin/env bash
set -u

JLINK_BIN_DIR="$HOME/.eclipse/com.st.stm32cube.ide.mcu.rcp.product_1.11.0_139060369_linux_gtk_x86_64/plugins/com.st.stm32cube.ide.mcu.externaltools.jlink.linux64_2.5.0.202506031126/tools/bin"
JLINK_SERVER="$JLINK_BIN_DIR/JLinkGDBServerCLExe"
JLINK_SERIAL="269304468"
DEVICE="STM32H750VB"
SPEED="4000"
GDB_PORT="19040"
SWO_PORT="19041"
TELNET_PORT="19042"
RTT_PORT="19043"
SERVER_NAME="CAN generator"
LOG_FILE="$HOME/jlink-gdb-generator.log"
PID_FILE="$HOME/jlink-gdb-generator.pid"
LEGACY_PID_FILE="$HOME/jlink-remote-generator.pid"

port_is_listening() {
    local port="$1"
    ss -ltn 2>/dev/null | grep -qE "LISTEN[[:space:]].*:${port}[[:space:]]"
}

find_server_pid() {
    local pid=""
    local cmd=""

    if [[ -f "$PID_FILE" ]]; then
        pid="$(cat "$PID_FILE" 2>/dev/null || true)"
        if [[ "$pid" =~ ^[0-9]+$ ]] && kill -0 "$pid" 2>/dev/null; then
            cmd="$(tr '\0' ' ' <"/proc/$pid/cmdline" 2>/dev/null || true)"
            if [[ "$cmd" == *"JLinkGDBServerCLExe"* ]] && \
               [[ "$cmd" == *"-USB $JLINK_SERIAL"* ]] && \
               [[ "$cmd" == *"-port $GDB_PORT"* ]]; then
                echo "$pid"
                return 0
            fi
        fi
        rm -f "$PID_FILE"
    fi

    pid="$(pgrep -f "JLinkGDBServerCLExe.*-USB ${JLINK_SERIAL}.*-port ${GDB_PORT}" | head -n 1 || true)"
    if [[ "$pid" =~ ^[0-9]+$ ]]; then
        echo "$pid"
        return 0
    fi

    return 1
}

is_running() {
    find_server_pid >/dev/null 2>&1 && port_is_listening "$GDB_PORT"
}

terminate_pid() {
    local pid="$1"

    if ! [[ "$pid" =~ ^[0-9]+$ ]] || ! kill -0 "$pid" 2>/dev/null; then
        return 0
    fi

    kill "$pid" 2>/dev/null || true

    for _ in {1..30}; do
        if ! kill -0 "$pid" 2>/dev/null; then
            return 0
        fi
        sleep 0.1
    done

    kill -KILL "$pid" 2>/dev/null || true
    sleep 0.2
}

cleanup_stale_gdb_instances() {
    local pid

    while read -r pid; do
        [[ -z "$pid" ]] && continue
        echo "Stopping stale ${SERVER_NAME} GDB Server process (PID ${pid})..."
        terminate_pid "$pid"
    done < <(pgrep -f "JLinkGDBServerCLExe.*-USB ${JLINK_SERIAL}.*-port ${GDB_PORT}" || true)

    rm -f "$PID_FILE"
}

cleanup_legacy_remote_server() {
    local pid
    local found=0

    while read -r pid; do
        [[ -z "$pid" ]] && continue
        found=1
        echo "Stopping legacy ${SERVER_NAME} J-Link Remote Server process (PID ${pid})..."
        terminate_pid "$pid"
    done < <(pgrep -f "JLinkRemoteServerCLExe.*-USB ${JLINK_SERIAL}" || true)

    if (( found != 0 )); then
        sleep 0.3
    fi
    rm -f "$LEGACY_PID_FILE"
}

check_ports_free() {
    local port

    for port in "$GDB_PORT" "$SWO_PORT" "$TELNET_PORT" "$RTT_PORT"; do
        if port_is_listening "$port"; then
            echo "Error: TCP port ${port} is already in use."
            ss -ltnp 2>/dev/null | grep ":${port}" || true
            return 1
        fi
    done

    return 0
}

start_server() {
    local pid

    if is_running; then
        pid="$(find_server_pid)"
        echo "${SERVER_NAME} J-Link GDB Server is already running (PID ${pid}, GDB port ${GDB_PORT})."
        return 0
    fi

    cleanup_stale_gdb_instances
    cleanup_legacy_remote_server

    if ! check_ports_free; then
        return 1
    fi

    if [[ ! -x "$JLINK_SERVER" ]]; then
        echo "Error: executable not found or not executable:"
        echo "  $JLINK_SERVER"
        return 1
    fi

    echo "Starting ${SERVER_NAME} J-Link GDB Server..."
    echo "Probe serial: ${JLINK_SERIAL}"
    echo "Device:       ${DEVICE}"
    echo "SWD speed:    ${SPEED} kHz"
    echo "GDB port:     ${GDB_PORT}"
    echo "SWO port:     ${SWO_PORT}"
    echo "Telnet port:  ${TELNET_PORT}"
    echo "RTT port:     ${RTT_PORT}"
    echo "Log:          ${LOG_FILE}"
    echo "PID file:     ${PID_FILE}"

    : >"$LOG_FILE"

    nohup "$JLINK_SERVER" \
        -USB "$JLINK_SERIAL" \
        -device "$DEVICE" \
        -endian little \
        -if SWD \
        -speed "$SPEED" \
        -port "$GDB_PORT" \
        -swoport "$SWO_PORT" \
        -telnetport "$TELNET_PORT" \
        -RTTTelnetPort "$RTT_PORT" \
        -LocalhostOnly 0 \
        -vd \
        >>"$LOG_FILE" 2>&1 </dev/null &

    pid=$!
    echo "$pid" >"$PID_FILE"

    for _ in {1..50}; do
        if is_running; then
            echo "${SERVER_NAME} J-Link GDB Server started successfully (PID ${pid})."
            ss -ltnp 2>/dev/null | grep -E ":(${GDB_PORT}|${SWO_PORT}|${TELNET_PORT}|${RTT_PORT})[[:space:]]" || true
            return 0
        fi

        if ! kill -0 "$pid" 2>/dev/null; then
            break
        fi
        sleep 0.1
    done

    echo "Error: ${SERVER_NAME} J-Link GDB Server did not start."
    echo "Stopping failed server process PID ${pid}..."
    terminate_pid "$pid"
    cleanup_stale_gdb_instances
    echo "Last log lines:"
    tail -n 40 "$LOG_FILE" 2>/dev/null || true
    return 1
}

stop_server() {
    local pid

    pid="$(find_server_pid || true)"

    if [[ -n "$pid" ]]; then
        echo "Stopping ${SERVER_NAME} J-Link GDB Server (PID ${pid})..."
        terminate_pid "$pid"
    else
        echo "${SERVER_NAME} J-Link GDB Server is not running."
    fi

    cleanup_stale_gdb_instances
    cleanup_legacy_remote_server
    rm -f "$PID_FILE"

    if port_is_listening "$GDB_PORT"; then
        echo "Error: GDB port ${GDB_PORT} is still in use."
        ss -ltnp 2>/dev/null | grep ":${GDB_PORT}" || true
        return 1
    fi

    echo "${SERVER_NAME} J-Link servers stopped."
    return 0
}

show_status() {
    local pid

    if is_running; then
        pid="$(find_server_pid)"
        echo "${SERVER_NAME} J-Link GDB Server is running."
        echo "Probe serial: ${JLINK_SERIAL}"
        echo "Device:       ${DEVICE}"
        echo "PID:          ${pid}"
        echo "GDB port:     ${GDB_PORT}"
        echo "SWO port:     ${SWO_PORT}"
        echo "Telnet port:  ${TELNET_PORT}"
        echo "RTT port:     ${RTT_PORT}"
        ss -ltnp 2>/dev/null | grep -E ":(${GDB_PORT}|${SWO_PORT}|${TELNET_PORT}|${RTT_PORT})[[:space:]]" || true
        return 0
    fi

    echo "${SERVER_NAME} J-Link GDB Server is not running."
    return 1
}

show_log() {
    touch "$LOG_FILE"
    tail -f "$LOG_FILE"
}

case "${1:-start}" in
    start)   start_server ;;
    stop)    stop_server ;;
    restart) stop_server && start_server ;;
    status)  show_status ;;
    log)     show_log ;;
    *)
        echo "Usage: $0 {start|stop|restart|status|log}"
        exit 2
        ;;
esac
