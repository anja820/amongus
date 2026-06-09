# Demo Flow: What to Run and What to Look For

This is the recommended 8-minute presentation flow.

The most important rule: **every command must answer an investigation question**.

---

## 0. Setup before your presentation

On the lab machine:

```bash
git clone <your-repo-url>
cd amongos_project
make
bash scripts/quick_test.sh
```

Have two terminals open:

- Terminal 1: run the spaceship program.
- Terminal 2: investigate using OS tools.

---

## 1. Launch the spaceship

Terminal 1:

```bash
bash scripts/run_demo.sh
```

What you look for:

```text
Controller PID: 4210
red    PID=4211 role=normal sleeper
blue   PID=4212 role=normal heartbeat
green  PID=4213 role=file descriptor baseline
yellow PID=4214 role=normal sleeper
purple PID=4215 role=normal heartbeat
cyan   PID=4216 role=COMBO impostor sabotage
```

What you say:

> The controller is the parent process. Every crewmate is a child process created with `fork()`. The class does not yet know which one is the Impostor.

Purpose:

- Introduces the story.
- Shows process creation.
- Gives you PIDs for later observation.

---

## 2. Build the suspect list

Terminal 2:

```bash
pstree -p <controller_pid>
```

Example output:

```text
amongos_ctrl(4210)-+-among_red(4211)
                   |-among_blue(4212)
                   |-among_green(4213)
                   |-among_yellow(4214)
                   |-among_purple(4215)
                   `-among_cyan(4216)
```

What you look for:

- One parent process.
- Several child processes.
- PIDs in parentheses.

What you say:

> This is the real process tree. These are not fake game characters; they are real Linux processes. The parent-child relationship is part of the process metadata maintained by the OS.

Purpose:

- Connects to PCB and process hierarchy.
- Shows the result of `fork()` visually.

---

## 3. Create the evidence board

Terminal 2:

```bash
ps -eo pid,ppid,stat,ni,pri,pcpu,pmem,comm | grep -E 'among_'
```

Example suspicious output:

```text
4211 4210 S   0  20   0.0  0.0 among_red
4212 4210 S   0  20   0.0  0.0 among_blue
4213 4210 S   0  20   0.0  0.0 among_green
4214 4210 S   0  20   0.0  0.0 among_yellow
4215 4210 S   0  20   0.0  0.0 among_purple
4216 4210 R   0  20  98.7  2.5 among_cyan
```

What you look for:

- `R` means runnable/running.
- `S` means sleeping.
- `%CPU` near 100 means CPU sabotage.
- `%MEM` higher than the others suggests memory sabotage.
- `PPID` should point back to the controller.

What you say:

> Cyan is suspicious because it is runnable and using much more CPU than the other crewmates. This means the scheduler keeps seeing it as ready to run.

Purpose:

- Shows process state.
- Shows scheduler-relevant behavior.
- Gives the first clue.

---

## 4. Confirm CPU sabotage

Terminal 2:

```bash
top -p <red_pid>,<blue_pid>,<green_pid>,<yellow_pid>,<purple_pid>,<cyan_pid>
```

What you look for:

- One process consistently near the top.
- `%CPU` much higher for one process.
- The process name `among_cyan`.

What you say:

> This process is CPU-bound. It is not waiting for I/O. It constantly competes for CPU time, so the scheduler must repeatedly decide when it runs and when it is preempted.

Purpose:

- Makes scheduling visible.
- Connects to CPU-bound workloads, context switches, and fairness.

Optional if installed:

```bash
pidstat -p <cyan_pid> 1
```

Look for:

- repeated CPU samples over time.
- more reliable evidence than a single `ps` snapshot.

---

## 5. Confirm memory sabotage

Terminal 2:

```bash
cat /proc/<cyan_pid>/status | grep -E 'State|VmSize|VmRSS|VmData'
```

Run it several times, or use:

```bash
watch -n 1 "cat /proc/<cyan_pid>/status | grep -E 'State|VmSize|VmRSS|VmData'"
```

What you look for:

```text
VmSize:   104000 kB
VmRSS:     32000 kB
VmData:    96000 kB
```

then later:

```text
VmSize:   160000 kB
VmRSS:     88000 kB
VmData:   152000 kB
```

Interpretation:

- `VmSize` = total virtual address space.
- `VmRSS` = resident set size; physical memory currently used.
- `VmData` = data/heap region.

What you say:

> The virtual memory and resident memory are growing because the Impostor keeps allocating heap memory and touching the pages. Touching pages matters because Linux often maps memory lazily; a page may not consume physical RAM until it is actually used.

Purpose:

- Shows memory management live.
- Connects `malloc()`, heap growth, virtual memory, and demand paging.

---

## 6. Confirm zombie bodies

Terminal 2:

```bash
ps -eo pid,ppid,stat,comm | grep -E 'Z|among_body'
```

Example output:

```text
4230 4216 Z among_body
4231 4216 Z among_body
```

What you look for:

- `STAT` contains `Z`.
- `PPID` points to the suspicious parent process.
- Command name is `among_body`.

What you say:

> These are zombie processes. They already exited, but the parent has not called `wait()`, so the kernel keeps a small process-table entry containing the exit status.

Purpose:

- Shows process death and missing `wait()`.
- Makes the zombie concept visible instead of just describing it.

---

## 7. Show file descriptors as a control example

The green process opens several harmless file descriptors.

Terminal 2:

```bash
ls -l /proc/<green_pid>/fd
```

What you look for:

```text
0 -> /dev/pts/1
1 -> /dev/pts/1
2 -> /dev/pts/1
3 -> /dev/null
4 -> /dev/null
5 -> /dev/null
6 -> /dev/null
```

What you say:

> `/proc` lets us inspect what the kernel knows about a process. File descriptors are part of the process state. This process is not the Impostor; it is here as a baseline to show that suspicious evidence must be interpreted carefully.

Purpose:

- Shows `/proc/<pid>/fd`.
- Avoids a one-dimensional demo.
- Gives a normal comparison process.

---

## 8. Demonstrate signals

Freeze a process:

```bash
kill -STOP <cyan_pid>
```

Check state:

```bash
ps -o pid,stat,comm -p <cyan_pid>
```

Look for:

```text
PID  STAT  COMMAND
4216 T     among_cyan
```

Resume it:

```bash
kill -CONT <cyan_pid>
```

Check again:

```bash
ps -o pid,stat,comm -p <cyan_pid>
```

What you say:

> `SIGSTOP` moves the process into a stopped state. `SIGCONT` allows it to continue. This shows that process state is controlled by the OS and represented in the process table.

Purpose:

- Shows signals.
- Shows `T` state.
- Gives a fun “freeze the Impostor” moment.

---

## 9. Emergency meeting

Say:

> We have three pieces of OS evidence: cyan used high CPU, cyan's memory grew, and cyan created zombie bodies. Therefore cyan is the Impostor.

Then reveal:

```bash
cat .amongos_state.txt
```

The state file shows the roles.

---

## 10. Cleanup

Terminal 1: press `Ctrl+C`, or from terminal 2:

```bash
bash scripts/cleanup.sh
```

Then verify:

```bash
ps -eo pid,ppid,stat,comm | grep among_
```

No output means cleanup succeeded.
