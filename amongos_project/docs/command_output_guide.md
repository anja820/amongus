# Command Output Guide

This file explains what each command does, why you run it, and what output you are looking for.

---

## `pstree -p <controller_pid>`

What it does:

- Displays the process tree starting from the controller process.
- `-p` shows PIDs.

Purpose:

- Proves that the crewmates are real child processes.
- Shows the parent-child relationship created by `fork()`.

Look for:

- `amongos_ctrl` as parent.
- `among_red`, `among_blue`, etc. as children.
- If zombie bodies exist, they may also appear under the Impostor.

OS concept:

- PCB metadata: PID, PPID, parent-child relationship.

---

## `ps -eo pid,ppid,stat,ni,pri,pcpu,pmem,comm | grep -E 'among_'`

What it does:

- Lists selected columns for every process.
- Filters to Among OS processes.

Purpose:

- Main evidence board for process state, CPU, memory, and priority.

Columns:

- `PID`: process ID.
- `PPID`: parent process ID.
- `STAT`: process state.
- `NI`: nice value.
- `PRI`: scheduling priority.
- `%CPU`: recent CPU usage.
- `%MEM`: memory usage percentage.
- `COMM`: command/process name.

Look for:

- `R`: running/runnable, suspicious if combined with high CPU.
- `S`: sleeping, normal for waiting processes.
- `T`: stopped after `SIGSTOP`.
- `Z`: zombie.
- high `%CPU`: CPU sabotage.
- rising `%MEM`: memory sabotage.

OS concept:

- Process states, scheduling, PCB fields exposed through OS tools.

---

## `top -p <pid1>,<pid2>,...`

What it does:

- Opens a live process monitor limited to selected PIDs.

Purpose:

- Confirms CPU sabotage live instead of relying on one snapshot.

Look for:

- One process repeatedly using much more CPU than others.
- In combo mode, `among_cyan` should be suspicious.

OS concept:

- Scheduling, CPU-bound workloads, fairness, context switching.

---

## `pidstat -p <pid> 1`

What it does:

- Prints CPU statistics for a PID every 1 second.

Purpose:

- More measurement-like than `top`; useful for Q&A or stronger evidence.

Look for:

- stable high `%CPU` over multiple samples.

OS concept:

- Scheduling metrics and repeated measurement.

Note:

- Requires `sysstat` package.

---

## `cat /proc/<pid>/status | grep -E 'State|VmSize|VmRSS|VmData'`

What it does:

- Reads kernel-exposed process information from `/proc`.
- Filters to process state and memory fields.

Purpose:

- Confirms memory sabotage.

Look for:

- `VmSize` increasing: virtual memory space grows.
- `VmRSS` increasing: physical RAM usage grows.
- `VmData` increasing: heap/data area grows.

OS concept:

- Virtual memory, resident memory, heap, demand paging.

Presentation line:

> `malloc()` reserves memory, but touching the pages is what makes RSS grow because Linux uses demand paging.

---

## `cat /proc/<pid>/maps`

What it does:

- Shows the virtual address space layout of a process.

Purpose:

- Supports the memory-management part if the professor asks for more depth.

Look for:

- executable mapping.
- heap region `[heap]`.
- shared libraries.
- stack region `[stack]`.

OS concept:

- Address-space regions, virtual memory, page mappings.

---

## `ls -l /proc/<pid>/fd`

What it does:

- Lists open file descriptors for a process.

Purpose:

- Shows that the OS tracks per-process resources.
- Demonstrates `/proc` beyond just CPU and memory.

Look for:

- `0`, `1`, `2` for stdin/stdout/stderr.
- extra descriptors pointing to `/dev/null` for the green baseline process.

OS concept:

- File descriptor table, process resources.

---

## `ps -eo pid,ppid,stat,comm | grep -E 'Z|among_body'`

What it does:

- Searches for zombie body processes.

Purpose:

- Confirms the zombie sabotage.

Look for:

- `STAT` containing `Z`.
- `PPID` pointing to the Impostor.
- command name `among_body`.

OS concept:

- `exit()` without parent `wait()` leaves a zombie entry.

---

## `kill -STOP <pid>`

What it does:

- Sends `SIGSTOP` to a process.
- This signal cannot be ignored by the process.

Purpose:

- Freezes the Impostor live.

Look for after running `ps`:

- `STAT` becomes `T`.

OS concept:

- Signals and stopped process state.

---

## `kill -CONT <pid>`

What it does:

- Sends `SIGCONT` to continue a stopped process.

Purpose:

- Resumes the frozen process.

Look for:

- `STAT` changes from `T` back to `R` or `S`.

OS concept:

- Signal-driven process state transitions.

---

## `kill -TERM <pid>`

What it does:

- Politely asks a process to terminate.

Purpose:

- Shows graceful process termination.

Look for:

- process disappears after cleanup.

OS concept:

- Signals and process lifecycle.

---

## `kill -KILL <pid>`

What it does:

- Immediately kills a process.
- The process cannot catch or ignore this signal.

Purpose:

- Backup cleanup if something does not stop.

Look for:

- process disappears.

OS concept:

- Kernel-enforced termination.

---

## `vmstat 1`

What it does:

- Prints system-wide CPU, memory, I/O, and paging activity once per second.

Purpose:

- Optional system-level view of memory pressure.

Look for:

- `si` / `so`: swap in/out. Ideally these stay 0 on a safe lab demo.
- `free`: free memory.
- `us`: user CPU time.

OS concept:

- System-wide memory and scheduling behavior.

---

## `valgrind --leak-check=full ./program`

What it does:

- Runs a program under Valgrind to detect memory errors and leaks.

Purpose:

- Optional Q&A or backup demo for memory bugs.

Look for:

- invalid write.
- definitely lost memory.
- heap summary.

OS concept:

- Heap correctness, invalid memory access, memory leak diagnosis.
