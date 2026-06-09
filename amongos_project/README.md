# Among OS: The Impostor Process

A live Linux OS-internals demo styled as **Among Us**.

One parent process starts six crewmate processes. Five behave normally. One process, **cyan**, is the hidden Impostor and sabotages the system by abusing CPU, leaking memory, and creating zombie child processes.

The class has to identify the Impostor using Linux observation tools such as `pstree`, `ps`, `top`, `/proc/<pid>/status`, `/proc/<pid>/fd`, signals, and optionally `valgrind`.

This project is designed for CM-204 Operating Systems Project 2 and connects:

- Session 3: processes, `fork()`, parent/child relationships, process states, zombies, signals, `/proc`
- Session 4: scheduling, runnable CPU-bound processes, priority/nice, CPU share
- Session 5: memory management, heap growth, virtual memory, RSS, page faults, memory bugs

---

## Repository contents

```text
amongos_project/
├── Makefile
├── README.md
├── PROJECT_PLAN.md
├── src/
│   └── amongos.c
├── scripts/
│   ├── run_demo.sh
│   ├── observe.sh
│   ├── cleanup.sh
│   ├── quick_test.sh
│   └── valgrind_leak_demo.sh
└── docs/
    ├── demo_flow.md
    ├── command_output_guide.md
    └── presenter_cheatsheet.md
```

---

## Requirements on Linux

Minimum:

```bash
sudo apt update
sudo apt install build-essential procps psmisc
```

Useful optional tools:

```bash
sudo apt install htop sysstat valgrind linux-tools-common
```

Notes:

- `pstree` comes from `psmisc`.
- `pidstat` comes from `sysstat`.
- `top`, `ps`, and `/proc` are normally already available.
- `perf` may require extra permissions depending on the lab machine.

---

## Quick start

Compile:

```bash
make
```

Run the main demo in terminal 1:

```bash
bash scripts/run_demo.sh
```

Observe from terminal 2:

```bash
bash scripts/observe.sh
```

Clean up if needed:

```bash
bash scripts/cleanup.sh
```

Run a short test:

```bash
bash scripts/quick_test.sh
```

---

## Recommended live presentation flow

### Terminal 1: start the spaceship

```bash
bash scripts/run_demo.sh
```

The program prints the controller PID and child PIDs. Do not reveal to the class that cyan is the Impostor until the end.

### Terminal 2: investigate

Start with:

```bash
bash scripts/observe.sh
```

Then manually run the most important commands:

```bash
pstree -p <controller_pid>
ps -eo pid,ppid,stat,ni,pri,pcpu,pmem,comm | grep -E 'among_'
top -p <pid1>,<pid2>,<pid3>,<pid4>,<pid5>,<pid6>
cat /proc/<suspect_pid>/status | grep -E 'State|VmSize|VmRSS|VmData|ctxt'
ps -eo pid,ppid,stat,comm | grep Z
ls -l /proc/<yellow_pid>/fd
```

### End: emergency meeting

Ask the class:

> Based on the OS evidence, which process is the Impostor?

Then reveal:

> Cyan was the Impostor.

Evidence should include:

- high `%CPU`
- increasing `VmRSS` / `VmData`
- zombie body processes with `STAT=Z`

---

## Useful command-line options

```bash
./amongos --help
```

Examples:

```bash
./amongos --sabotage cpu
./amongos --sabotage memory --mem-step-mb 4 --mem-limit-mb 64
./amongos --sabotage zombie --zombie-limit 3
./amongos --sabotage combo --mem-step-mb 8 --mem-limit-mb 256 --zombie-limit 5
./amongos --sabotage combo --duration 60
```

For live demos, use the default combo mode but keep memory capped.

---

## Safety notes

The program intentionally creates suspicious behavior, but it is capped:

- Memory growth defaults to a 256 MB limit.
- Zombie bodies default to 5.
- `cleanup.sh` stops remaining demo processes.

Do **not** remove the memory cap on a shared lab machine.

---

## Presentation idea in one sentence

> We made process, scheduling, and memory internals visible by turning Linux processes into Among Us crewmates and catching the Impostor using real OS evidence.
