#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
make clean
make
./amongos --sabotage combo --duration 8 --mem-step-mb 4 --mem-limit-mb 16 --zombie-limit 1 > /tmp/amongos_quick_test.log 2>&1 || true
grep -q "AMONG OS" /tmp/amongos_quick_test.log
echo "Quick test passed. Log: /tmp/amongos_quick_test.log"
