# Among OS — Presentation Cheat Sheet
CM-204 Operating Systems · Project 2 · Max 8 minutes

---

## Setup (do this before entering the room)

```bash
# 1. Build everything
make all

# 2. Verify it runs
./amongos --sabotage combo --duration 5 --reveal

# 3. Make scripts executable
chmod +x scripts/*.sh

# 4. Open 3 terminal windows (split screen or tmux)
#    Terminal A  →  main demo (Speaker 1)
#    Terminal B  →  process + scheduler commands (Speaker 1 & 2)
#    Terminal C  →  memory + valgrind commands (Speaker 3)
```

---

## Speaker 1 — Process Detective (0:00 – 2:30)

**Your job:** launch the demo, show the process family, explain PCB concepts, reveal zombies, demonstrate signals.

### Step 1 — launch (Terminal A)

```bash
bash scripts/run_demo.sh
```

What it prints: controller PID, all child PIDs, and pre-filled investigation commands.

> *"Every name you see is a real Linux process. The OS tracks each one in a kernel structure called the PCB — process control block. Linux exposes parts of it through `/proc` and `ps`."*

---

### Step 2 — process tree (Terminal B)

```bash
pstree -p <controller_pid>
```

What to look for:
- All 6 crewmates branching off the controller.
- If zombie mode: cyan has extra `among_body` children beneath it.
- If orphan mode: `among_orphan` processes appear directly under the controller or PID 1.

> *"This tree is the result of `fork()`. The controller is the parent; each crewmate is a child. `pstree` reads `/proc/<pid>/status` for every process — that's the PCB pathway."*

---

### Step 3 — full evidence board (Terminal B)

```bash
ps -eo pid,ppid,stat,ni,pcpu,pmem,comm | grep -E 'among_|PID'
```

What to look for:

| STAT | Meaning | What to say |
|------|---------|-------------|
| `S`  | Sleeping (interruptible) | "Normal crewmate, waiting on I/O or timer" |
| `R`  | Runnable / running | "This process keeps asking the scheduler for CPU" |
| `Z`  | Zombie | "Process exited but parent never called wait()" |
| `T`  | Stopped | "Frozen by SIGSTOP — no CPU, no scheduling" |

---

### Step 4 — /proc deep dive (Terminal B)

```bash
cat /proc/<suspect_pid>/status | grep -E 'Name|State|Pid|PPid|VmSize|VmRSS|VmData'
```

> *"`/proc` is not a real directory on disk. The kernel generates it on-the-fly. What we see here comes directly from the process table — the user-visible shadow of the PCB."*

---

### Step 5 — zombie investigation (Terminal B)

```bash
# Find zombies
ps -eo pid,ppid,stat,comm | awk '$3 ~ /Z/'

# Check a zombie's status (PCB entry still exists)
cat /proc/<zombie_pid>/status | grep -E 'State|PPid'
```

> *"The process is done executing but the OS keeps a minimal PCB entry so the parent can collect the exit status via `wait()`. This is the zombie state Z."*

---

### Step 6 — signal demo (Terminal B)

```bash
# Freeze a crewmate
kill -STOP <pid>
ps -o pid,stat,comm -p <pid>   # STAT shows T

# Resume it
kill -CONT <pid>
ps -o pid,stat,comm -p <pid>   # STAT shows S or R again

# Polite kill (catchable)
kill -TERM <pid>

# Forced kill (cannot be caught or ignored)
kill -KILL <pid>
```

> *"Signals are the OS mechanism for sending asynchronous notifications to processes. `SIGKILL` cannot be caught — the kernel enforces it directly."*

---

## Speaker 2 — Scheduler Detective (2:30 – 5:00)

**Your job:** show CPU-bound vs sleeping processes, use top/pidstat, change nice value live, tie to MLFQ/CFS.

### Step 7 — live CPU comparison (Terminal B)

```bash
# Watch all six crewmates side-by-side
top -p <pid1>,<pid2>,<pid3>,<pid4>,<pid5>,<pid6>
```

What to look for:
- Cyan near 90–100 %CPU (combo/cpu mode).
- Others near 0 %CPU (sleeping).
- Cyan's STAT column stays `R`; others show `S`.

> *"A CPU-bound process never voluntarily gives up the CPU. It stays in the runnable queue. The scheduler gives it a time slice, it uses all of it, gets preempted, and immediately wants another one. That's the turnaround vs response tension from the MLFQ discussion."*

---

### Step 8 — per-second measurement (Terminal B)

```bash
pidstat -p <cyan_pid> 1
```

Why this is better than `top`: gives line-by-line evidence, not just a snapshot.

> *"One measurement can be noise. `pidstat` gives us repeated samples — much stronger evidence than a single screenshot."*

---

### Step 9 — context switches (Terminal B)

```bash
grep -E 'voluntary_ctxt|nonvoluntary' /proc/<cyan_pid>/status
grep -E 'voluntary_ctxt|nonvoluntary' /proc/<normal_pid>/status
```

What to look for:
- Cyan: high **nonvoluntary** switches (preempted by the scheduler).
- Normal crewmates: high **voluntary** switches (they sleep willingly).

> *"Voluntary context switches mean the process blocked (I/O, sleep). Nonvoluntary means the scheduler forcibly took the CPU away after the time quantum expired — exactly the timer-interrupt preemption from the lecture."*

---

### Step 10 — change priority live (Terminal B)

```bash
# Reduce cyan's scheduling priority
renice +10 -p <cyan_pid>

# Verify the change
ps -o pid,ni,pri,pcpu,comm -p <cyan_pid>
```

> *"We're not killing the impostor — we're making it nicer. A higher nice value lowers the priority. Under CPU competition, Linux's CFS gives it proportionally less CPU time. This is the user-space knob into the scheduler."*

---

### Step 11 — strace syscall crossing (Terminal B, optional / backup)

```bash
# Run standalone strace demo (takes ~20 s)
bash scripts/strace_demo.sh
```

Or attach live:

```bash
strace -f -e trace=clone,fork,wait4 -p <cyan_pid> 2>&1 | head -20
```

> *"Our C code can't create processes by editing kernel data structures — it has to ask the kernel. The `clone()` syscall is the trap that crosses the user/kernel boundary. `strace` intercepts every such crossing."*

---

## Speaker 3 — Memory Detective (5:00 – 7:30)

**Your job:** show virtual vs physical memory, growing VmRSS, heap in /proc/maps, vmstat, run valgrind demo.

### Step 12 — watch memory grow (Terminal C)

```bash
watch -n1 'grep -E "VmSize|VmRSS|VmData" /proc/<cyan_pid>/status'
```

What to look for while it runs:
- `VmData` grows every ~2 s (heap allocation).
- `VmRSS` grows at the same rate (pages are touched → resident in RAM).
- `VmSize` ≥ `VmRSS` (virtual ≥ physical — always true).

> *"VmSize is the virtual address space — what the process can see. VmRSS is what's actually in physical RAM. They differ because of demand paging: the OS only brings a page into RAM when the process first touches it."*

---

### Step 13 — address-space layout (Terminal C)

```bash
cat /proc/<cyan_pid>/maps | grep -E 'heap|stack|\.so'
```

Annotate each region out loud:

| Region | What it is |
|--------|-----------|
| `[heap]` | dynamic allocations (`malloc`) — grows upward |
| `[stack]` | call stack — grows downward |
| `libc.so` | shared library — mapped into VA space via page table |
| `r-xp` (text) | executable code — read+execute, copy-on-write |

> *"This is the process's virtual address space. Each row is a range of virtual page numbers pointing into physical frames via the page table. The OS manages these PTEs invisibly — until a page fault forces it to bring a new frame in."*

---

### Step 14 — system-wide memory pressure (Terminal C)

```bash
vmstat 1
```

Key columns:

| Field | Meaning | Watch for |
|-------|---------|-----------|
| `r` | processes in run queue | rises when CPU-bound impostor runs |
| `si`/`so` | swap in / swap out | non-zero means memory pressure |
| `us` | user-space CPU % | high when CPU impostor runs |
| `id` | idle % | drops when impostor competes |

> *"vmstat shows the whole ship's health. If `so` becomes non-zero, pages are being evicted to disk — that's the page-replacement policy kicking in."*

---

### Step 15 — valgrind memory autopsy (Terminal C)

```bash
bash scripts/valgrind_demo.sh
```

This compiles `src/buggy_crewmate.c` and runs it under valgrind. Four errors appear:

| Valgrind report | Bug | OS / heap concept |
|-----------------|-----|-------------------|
| `Invalid write of size 1` | Heap overflow | Write past PTE-backed allocation |
| `definitely lost: 4096 bytes` | Memory leak | VA range never freed / unmapped |
| `Invalid read of size 1` | Use-after-free | PTE still mapped, content undefined |
| `Uninitialised value` | Uninitialized read | Frame present but contents not zeroed |

> *"The OS doesn't know these are bugs — it just maps pages. Valgrind instruments every memory access in userspace and catches when we violate the allocator's contract. Each error class maps directly to a concept from Session 5."*

---

### Step 15b — TLB sabotage demo (Terminal C, run instead of or after valgrind)

Start the demo in TLB mode:

```bash
./amongos --sabotage tlb
```

Cyan will print a round every ~2 seconds showing sequential vs random access time:

```
[HH:MM:SS] cyan   PID=…  | round 1 | seq 4 ms | random 31 ms | 7.8x slower – random blows TLB capacity…
```

Watch it live with perf (needs `perf_event_paranoid ≤ 1`):

```bash
# Check first:
cat /proc/sys/kernel/perf_event_paranoid

# If 1 or lower:
perf stat -e dTLB-misses,dTLB-loads -p <cyan_pid> sleep 10
```

| Metric | Sequential | Random |
|--------|-----------|--------|
| `dTLB-misses` | low (one miss per new page, then hits) | very high (almost every access) |
| Slowdown | baseline | 5–10× slower |

> *"Both scans touch the same 8192 pages. Sequential order gives the TLB time to cache each page's translation before moving on. Random order — stride 4099 pages — guarantees we evict a TLB entry before reusing it. Every random access pays a full page-table walk: VA → PGD → PUD → PMD → PTE → physical frame. That's the TLB miss penalty made visible."*

---

## Emergency Meeting & Reveal (7:30 – 8:00)

### Trigger the meeting screen

From Terminal B:

```bash
bash scripts/emergency_meeting.sh
```

This sends `SIGUSR1` to the controller. The main terminal prints the meeting banner with all PIDs.

### Reveal the impostor

```bash
bash scripts/emergency_meeting.sh --reveal
```

Sends `SIGUSR2` — same banner plus "cyan was The Impostor".

> *"We caught the impostor using only Linux OS observation tools — no guessing, no slides. PCB-visible process state, scheduler measurements, virtual memory growth, and heap-bug diagnosis."*

---

## Cleanup

```bash
bash scripts/cleanup.sh

# Verify nothing remains
ps -eo pid,ppid,stat,comm | grep -E 'among_|amongos'
```

---

## Q&A Backup Commands

```bash
# Page faults on the impostor process
perf stat -e page-faults -p <cyan_pid>   # needs perf installed

# Orphan demo (run separately)
./amongos --sabotage orphan --duration 30
# Then: watch -n1 'grep PPid /proc/<orphan_pid>/status'
# You will see PPid change to 1 (or systemd's subreaper PID)

# Full address space (unfiltered — verbose, use in Q&A only)
cat /proc/<pid>/maps

# Full strace output (verbose)
strace -f ./amongos --sabotage zombie --zombie-limit 2 --duration 8

# File descriptors for yellow (fd baseline crewmate)
ls -l /proc/<yellow_pid>/fd
readlink /proc/<yellow_pid>/fd/3
```

---

## Quick concept answers for professor questions

**"Where is the PCB?"**  
Kernel data structure — not directly printable from user space. Linux exposes selected fields via `/proc/<pid>/status` and `ps`. What we're reading IS the PCB, just through a safe interface.

**"Why does a zombie still show in ps?"**  
The process finished executing but the kernel keeps a minimal process-table entry so the parent can collect the exit status with `wait()`. Once the parent waits (or dies), the zombie disappears.

**"Why doesn't malloc immediately increase RSS?"**  
`malloc` reserves virtual address space (VmSize grows) but the OS uses demand paging: physical frames are only assigned when the page is first written. Our `memset` forces that touch — that's why RSS grows after `memset`, not after `malloc`.

**"Why can't SIGKILL be caught?"**  
The signal is delivered and acted on entirely within the kernel — the process never gets a chance to run a handler. It's a kernel-enforced, non-bypassable operation.

**"Is this MLFQ?"**  
Not exactly. Linux uses CFS (Completely Fair Scheduler) not a pure MLFQ. But the observable behaviours — CPU-bound processes getting demoted/penalised, interactive processes staying responsive — relate directly to the trade-offs MLFQ was designed to solve.

**"What are the nonvoluntary context switches?"**  
The timer interrupt fires, the kernel runs the scheduler, and a different process is chosen. The current process didn't yield — it was preempted. Each nonvoluntary switch is one timer-interrupt preemption.
