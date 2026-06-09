#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
echo "Stopping remaining Among OS processes..."
pkill -TERM -f './amongos' 2>/dev/null || true
pkill -TERM -f 'among_' 2>/dev/null || true
sleep 1
pkill -KILL -f './amongos' 2>/dev/null || true
pkill -KILL -f 'among_' 2>/dev/null || true
rm -f .amongos_controller.pid
echo "Cleanup attempted. Check with: ps -eo pid,ppid,stat,comm | grep among_"
