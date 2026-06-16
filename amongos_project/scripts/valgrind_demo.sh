#!/usr/bin/env bash
# Speaker 3 — valgrind memory-bug autopsy demo.
# Shows 4 bug classes: heap overflow, leak, use-after-free, uninit read.
set -euo pipefail
cd "$(dirname "$0")/.."

if ! command -v valgrind >/dev/null 2>&1; then
    echo "valgrind not found. Install: sudo apt install valgrind"
    exit 1
fi

make -s buggy_crewmate

echo ""
echo "═══════════════════════════════════════════════════════════════"
echo " VALGRIND MEMORY BUG AUTOPSY"
echo " Each error class maps to a heap / page-table concept."
echo "═══════════════════════════════════════════════════════════════"
echo ""

valgrind \
    --leak-check=full \
    --track-origins=yes \
    --show-leak-kinds=all \
    --error-exitcode=0 \
    ./buggy_crewmate

echo ""
echo "═══════════════════════════════════════════════════════════════"
echo " Bug 1 overflow  → 'Invalid write'  — heap metadata corrupted"
echo " Bug 2 leak      → 'definitely lost' — VAs never unmapped"
echo " Bug 3 UAF       → 'Invalid read'   — freed PTE still accessed"
echo " Bug 4 uninit    → 'Uninitialised'  — page present, value random"
echo "═══════════════════════════════════════════════════════════════"
