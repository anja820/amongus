#!/usr/bin/env bash
# Send SIGUSR1 to the controller to trigger the emergency-meeting screen.
# With --reveal flag, sends SIGUSR2 instead (also prints the impostor).
set -euo pipefail
cd "$(dirname "$0")/.."

if [[ ! -f .amongos_controller.pid ]]; then
    echo "No running demo found. Start with: bash scripts/run_demo.sh"
    exit 1
fi
CTRL=$(cat .amongos_controller.pid)

if [[ "${1:-}" == "--reveal" ]]; then
    echo "Sending SIGUSR2 (meeting + reveal) to PID $CTRL …"
    kill -USR2 "$CTRL"
else
    echo "Sending SIGUSR1 (meeting screen) to PID $CTRL …"
    kill -USR1 "$CTRL"
fi
