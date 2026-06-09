#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
make
rm -f .amongos_state.txt .amongos_controller.pid
printf '\nStarting Among OS demo. Open another terminal and run: bash scripts/observe.sh\n\n'
./amongos --sabotage combo --mem-step-mb 8 --mem-limit-mb 256 --zombie-limit 5
