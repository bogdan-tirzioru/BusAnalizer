#!/usr/bin/env bash
set -u

JLINK_SERVER="$HOME/.eclipse/com.st.stm32cube.ide.mcu.rcp.product_1.11.0_139060369_linux_gtk_x86_64/plugins/com.st.stm32cube.ide.mcu.externaltools.jlink.linux64_2.5.0.202506031126/tools/bin/JLinkRemoteServerCLExe"
JLINK_SERIAL="269304468"
PORT="19040"
SERVER_NAME="CAN generator"
LOG_FILE="$HOME/jlink-remote-generator.log"
PID_FILE="$HOME/jlink-remote-generator.pid"

find_server_pid() {
    local pid=""

    if [[ -f "$PID_FILE" ]]; then
        pid="$(cat "$PID_FILE" 2>/dev/null || true)"
        if [[ "$pid" =~ ^[0-9]+$ ]] && kill -0 "$pid" 2>/dev/null; then
            if tr '\0' ' ' <"/proc/$pid/cmdline" 2>/dev/null | \
               grep -Fq -- "-USB $JLINK_SERIAL"; then
                echo "$pid"
                return 0
            fi
        fi
        rm -f "$PID_FILE"
    fi

    pid="$(pgrep -f "JLinkRemoteServerCLExe.*-USB ${JLINK_SERIAL}.*-Port ${PORT}" | head -n 1 || true)"
    if [[ "$pid" =~ ^[0-9]+$ ]]; then
        echo "$pid"
        return 0
    fi

    return 1
}

is_listening() {
    ss -ltn 2>/dev/null | grep -qE "LISTEN[[:space:]].*:${PORT}[[:space:]]"
}

is_running() {
    find_server_pid >/dev/null 2>&1 && is_listening
}

terminate_pid() {
    local pid="$1"

    if ! [[ "$pid" =~ ^[0-9]+$ ]] || ! kill -0 "$pid" 2>/dev/null; then
        return 0
    fi

    kill "$pid" 2>/dev/null || true

    for _ in {1..20}; do
        if ! kill -0 "$pid" 2>/dev/null; then
            return 0
        fi
        sleep 0.1
    done

    kill -KILL "$pid" 2>/dev/null || true
    sleep 0.2
}

cleanup_stale_instances() {
    local pid
    local found=0

    while read -r pid; do
        [[ -z "$pid" ]] && continue
        found=1
        echo "Stopping stale ${SERVER_NAME} Remote Server process (PID ${pid})..."
        terminate_pid "$pid"
    done < <(pgrep -f "JLinkRemoteServerCLExe.*-USB ${JLINK_SERIAL}.*-Port ${PORT}" || true)

    if (( found != 0 )); then
        rm -f "$PID_FILE"
    fi
}

start_server() {
    local pid

    if is_running; then
        pid="$(find_server_pid)"
        echo "${SERVER_NAME} J-Link Remote Server is already running (PID ${pid}, port ${PORT})."
        return 0
    fi

    if is_listening; then
        echo "Error: port ${PORT} is already in use by another process."
        ss -ltnp 2>/dev/null | grep ":${PORT}" || true
        return 1
    fi

    cleanup_stale_instances

    if [[ ! -x "$JLINK_SERVER" ]]; then
        echo "Error: executable not found or not executable:"
        echo "  $JLINK_SERVER"
        return 1
    fi

    echo "Starting ${SERVER_NAME} J-Link Remote Server..."
    echo "Probe serial: ${JLINK_SERIAL}"
    echo "Port:         ${PORT}"
    echo "Log:          ${LOG_FILE}"
    echo "PID file:     ${PID_FILE}"

    nohup "$JLINK_SERVER" \
        -USB "$JLINK_SERIAL" \
        -Port "$PORT" \
        >>"$LOG_FILE" 2>&1 </dev/null &

    pid=$!
    echo "$pid" >"$PID_FILE"

    sleep 2

    if is_running; then
        echo "${SERVER_NAME} J-Link Remote Server started successfully (PID ${pid})."
        ss -ltnp 2>/dev/null | grep ":${PORT}" || true
        return 0
    fi

    echo "Error: ${SERVER_NAME} J-Link Remote Server did not start."
    echo "Stopping failed server process PID ${pid} so it cannot keep the probe busy..."
    terminate_pid "$pid"
    rm -f "$PID_FILE"
    echo "Last log lines:"
    tail -n 30 "$LOG_FILE" 2>/dev/null || true
    return 1
}

stop_server() {
    local pid

    pid="$(find_server_pid || true)"

    if [[ -z "$pid" ]]; then
        echo "${SERVER_NAME} J-Link Remote Server is not running."
        cleanup_stale_instances
        rm -f "$PID_FILE"
        return 0
    fi

    echo "Stopping ${SERVER_NAME} J-Link Remote Server (PID ${pid})..."
    terminate_pid "$pid"
    cleanup_stale_instances
    rm -f "$PID_FILE"

    if is_listening; then
        echo "Error: port ${PORT} is still in use."
        ss -ltnp 2>/dev/null | grep ":${PORT}" || true
        return 1
    fi

    echo "${SERVER_NAME} J-Link Remote Server stopped."
    return 0
}

show_status() {
    local pid

    if is_running; then
        pid="$(find_server_pid)"
        echo "${SERVER_NAME} J-Link Remote Server is running."
        echo "Probe serial: ${JLINK_SERIAL}"
        echo "PID:          ${pid}"
        echo "Port:         ${PORT}"
        ss -ltnp 2>/dev/null | grep ":${PORT}" || true
        return 0
    fi

    echo "${SERVER_NAME} J-Link Remote Server is not running."
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
