#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

if [[ ! -f .amongos_controller.pid ]]; then
  echo "No .amongos_controller.pid found. Start the demo first: bash scripts/run_demo.sh"
  exit 1
fi
CTRL=$(cat .amongos_controller.pid)
echo "Controller PID: $CTRL"
echo

echo "1) Process family tree: who created whom?"
pstree -p "$CTRL" || true
echo

echo "2) Main evidence board: look for STAT, %CPU, %MEM"
ps -eo pid,ppid,stat,ni,pri,pcpu,pmem,comm | grep -E 'among_|PID' || true
echo

echo "3) Zombie bodies, if any: look for STAT containing Z"
ps -eo pid,ppid,stat,comm | awk '$3 ~ /Z/ || NR==1 {print}' | grep -E 'among_|PID|body|STAT' || true
echo

echo "4) Memory suspects from the state file: use /proc/<pid>/status"
if [[ -f .amongos_state.txt ]]; then
  cat .amongos_state.txt
  echo
  awk 'NF >= 3 && $1 != "#" && $1 != "controller" {print $1, $2}' .amongos_state.txt | while read -r color pid; do
    if [[ -r "/proc/$pid/status" ]]; then
      echo "--- $color PID=$pid ---"
      grep -E 'State|VmSize|VmRSS|VmData|Threads|voluntary_ctxt_switches|nonvoluntary_ctxt_switches' "/proc/$pid/status" || true
    fi
  done
fi

echo
echo "For live updating CPU view, run one of these manually:"
echo "  top -p \\$(awk 'NF>=3 && \\$1 != \"#\" && \\$1 != \"controller\" {print \\$2}' .amongos_state.txt | paste -sd, -)"
echo "  pidstat -p ALL 1   # if sysstat/pidstat is installed"
