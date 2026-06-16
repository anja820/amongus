#!/usr/bin/env bash
# Speaker 2 — strace syscall demo.
# Shows the user→kernel boundary crossings during process creation and memory growth.
set -euo pipefail
cd "$(dirname "$0")/.."

make -s all

echo ""
echo "═══════════════════════════════════════════════════════════════"
echo " STRACE DEMO 1: process-creation syscalls (zombie mode, 8 s)"
echo " Shows: clone/fork, wait4 — the kernel side of fork() / wait()"
echo "═══════════════════════════════════════════════════════════════"
echo ""
strace -f -e trace=clone,fork,vfork,execve,wait4,exit_group \
       ./amongos --sabotage zombie --zombie-limit 2 --duration 8 2>&1 \
| head -80 || true

echo ""
echo "═══════════════════════════════════════════════════════════════"
echo " STRACE DEMO 2: memory syscalls (memory mode, 10 s)"
echo " Shows: brk / mmap — how malloc asks kernel for more heap"
echo "═══════════════════════════════════════════════════════════════"
echo ""
strace -f -e trace=brk,mmap,munmap \
       ./amongos --sabotage memory --mem-step-mb 4 --mem-limit-mb 32 --duration 10 2>&1 \
| head -80 || true

echo ""
echo "Key insight: malloc() is a library call, but when the allocator"
echo "needs more address space it uses brk() or mmap() — both are"
echo "privileged kernel operations entered via a trap (syscall instruction)."
