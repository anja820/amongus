# Among Us: The Impostor Process

**CM-204 Operating Systems — Project 2 Proposal**  
**Theme:** A live terminal investigation game where one Linux process is secretly sabotaging the system.

---

## 1. Core Idea

Instead of presenting three disconnected demos about processes, scheduling, and memory, we turn the whole presentation into a short live investigation:

> A spaceship is running several normal crew processes. One process is the Impostor. The Impostor is secretly damaging the system by abusing CPU, leaking memory, spawning zombie children, or forking suspicious processes. The team must identify it using Linux OS-observation tools.

The class sees a fun story, but the professor sees a serious OS-internals demonstration:

- Process creation and parent-child relationships
- PCB-visible state through `ps`, `pstree`, and `/proc/<pid>/status`
- Scheduling behavior through `top`, `pidstat`, `nice`, and CPU-bound workloads
- Memory behavior through `malloc`, `/proc/<pid>/maps`, `/proc/<pid>/status`, `vmstat`, and `valgrind`
- Signals through `SIGTERM`, `SIGKILL`, `SIGSTOP`, and `SIGCONT`
- Zombies through missing `wait()`

---

## 2. One-Sentence Pitch

**“We built a mini Among Us game in C where each crewmate is a real Linux process, and we use OS tools to catch the Impostor by observing process state, CPU scheduling, and memory behavior live.”**

---

## 3. Why This Has a Wow Factor

Most groups will probably show a simple zombie process, a basic `fork()` example, or a memory leak in isolation.

This project is stronger because:

1. It has a story the audience understands immediately.
2. It is interactive: the class can guess who the Impostor is.
3. Every “game mechanic” maps to a real OS concept.
4. The demo is live in the terminal, not just slides.
5. It naturally combines all three required areas: processes, scheduling, and memory.

---

## 4. Project Architecture

The project can be implemented as a small C program called:

```bash
./amongos
```

The program starts a parent process called the **Spaceship Controller**.

The controller forks several child processes:

```text
Spaceship Controller
├── Crewmate_Red
├── Crewmate_Blue
├── Crewmate_Green
├── Crewmate_Yellow
├── Crewmate_Purple
└── Impostor_Unknown
```

Each child process performs a different workload.

---

## 5. Process Roles

| Process | Behavior | OS Concept |
|---|---|---|
| Red | Sleeps and wakes periodically | Interruptible sleep, I/O-like behavior |
| Blue | CPU-bound calculation loop | Scheduling, CPU share, context switches |
| Green | Allocates memory slowly | Heap growth, virtual memory, page faults |
| Yellow | Opens files and waits | File descriptors, `/proc/<pid>/fd` |
| Purple | Normal harmless process | Baseline comparison |
| Impostor | Randomly performs sabotage | Investigation target |

The Impostor can be configured to perform one of several sabotage types.

---

## 6. Sabotage Types

### Sabotage A: CPU Sabotage

The Impostor runs an infinite CPU loop:

```c
while (1) {
    x = x * 3.14159;
}
```

**Observation tools:**

```bash
top -p <pid>
pidstat -p <pid> 1
ps -o pid,ppid,stat,ni,pri,pcpu,comm -p <pid>
```

**Concepts explained:**

- CPU-bound process
- Scheduler decisions
- Nice value and priority
- Response time vs CPU usage
- Relation to MLFQ and Linux CFS

---

### Sabotage B: Memory Leak Sabotage

The Impostor repeatedly allocates memory and never frees it:

```c
while (1) {
    void *p = malloc(1024 * 1024);
    memset(p, 1, 1024 * 1024);
    sleep(1);
}
```

Touching the memory using `memset()` is important because it forces physical pages to be committed.

**Observation tools:**

```bash
cat /proc/<pid>/status | grep -E "VmSize|VmRSS|VmData"
cat /proc/<pid>/maps
vmstat 1
free -h
perf stat -e page-faults -p <pid>
```

**Concepts explained:**

- Virtual memory vs physical memory
- Heap growth
- Demand paging
- Page faults
- Page table entries
- Why `malloc()` alone may not immediately increase RSS

---

### Sabotage C: Zombie Sabotage

The Impostor forks child processes, but the parent never calls `wait()`:

```c
pid_t pid = fork();
if (pid == 0) {
    exit(0);
}
// Parent intentionally does not call wait()
sleep(60);
```

**Observation tools:**

```bash
ps -o pid,ppid,stat,comm
pstree -p
cat /proc/<zombie_pid>/status
```

**Concepts explained:**

- Zombie process state `Z`
- Parent-child relationship
- Exit status waiting in the kernel
- PCB still exists even though process execution is finished
- Why `wait()` cleans up the zombie

---

### Sabotage D: Signal Sabotage

The controller or presenter sends signals to processes:

```bash
kill -STOP <pid>
kill -CONT <pid>
kill -TERM <pid>
kill -KILL <pid>
```

**Observation tools:**

```bash
ps -o pid,ppid,stat,comm -p <pid>
```

**Concepts explained:**

- `SIGSTOP` creates stopped state `T`
- `SIGCONT` resumes execution
- `SIGTERM` asks a process to terminate gracefully
- `SIGKILL` cannot be caught or ignored
- Signals as OS-level process control

---

## 7. Recommended Final Demo Design

Use exactly **one main Impostor behavior** during the live presentation, but mention that the code supports multiple sabotage modes.

The best live combination is:

1. CPU abuse
2. Memory leak
3. Zombie creation

This gives the strongest integration of all three sessions.

Recommended command:

```bash
./amongos --impostor random
```

Alternative deterministic command for safe demo practice:

```bash
./amongos --impostor memory
./amongos --impostor cpu
./amongos --impostor zombie
```

For presentation day, deterministic mode is safer than true randomness.

---

## 8. Suggested 8-Minute Presentation Script

The assignment has a strict 8-minute timebox, so the demo must be fast and rehearsed.

### 0:00–0:45 — Opening Hook

Presenter 1 says:

> “We turned Linux process inspection into Among Us. Each crewmate is a real process. One process is the Impostor. We will catch it using only OS tools.”

Run:

```bash
./amongos --impostor memory
```

Show the program printing:

```text
[SPACESHIP] Launching crew...
[RED] Doing tasks...
[BLUE] Calculating engine vectors...
[GREEN] Monitoring oxygen...
[YELLOW] Checking files...
[PURPLE] Waiting...
[???] One process is the Impostor.
```

---

### 0:45–2:00 — Process Investigation

Presenter 1 shows:

```bash
pstree -p <controller_pid>
ps -o pid,ppid,stat,comm
```

Explain:

- The controller is the parent process.
- Each crewmate is a child process created by `fork()`.
- The PCB stores process information such as PID, PPID, state, and scheduling information.
- `/proc` exposes parts of this information to user space.

Audience sees that the “spaceship crew” is actually a real Linux process tree.

---

### 2:00–3:30 — Scheduling Investigation

Presenter 2 checks CPU usage:

```bash
top -p <pid1>,<pid2>,<pid3>,<pid4>,<pid5>,<pid6>
```

or:

```bash
pidstat -p ALL 1
```

Explain:

- One process may be CPU-bound.
- CPU-bound processes constantly want the CPU.
- I/O-like processes sleep and wake up.
- The scheduler decides who runs next.
- Nice values can influence CPU share.

Optional live command:

```bash
renice +10 -p <suspicious_pid>
```

Then show CPU share changing.

---

### 3:30–5:30 — Memory Investigation

Presenter 3 checks memory:

```bash
cat /proc/<suspect_pid>/status | grep -E "VmSize|VmRSS|VmData"
watch -n 1 'cat /proc/<suspect_pid>/status | grep -E "VmSize|VmRSS|VmData"'
```

Then:

```bash
cat /proc/<suspect_pid>/maps | grep heap
vmstat 1
```

Explain:

- `VmSize` is virtual memory size.
- `VmRSS` is resident physical memory.
- Heap grows because the process calls `malloc()`.
- RSS grows only when pages are touched.
- Page faults occur when virtual pages are mapped to physical frames.

This is one of the strongest technical moments.

---

### 5:30–6:45 — Zombie Evidence

If using zombie mode or combined mode:

```bash
ps -o pid,ppid,stat,comm | grep Z
pstree -p
```

Explain:

- A zombie process has finished execution.
- It still has an entry in the process table.
- The parent did not call `wait()`.
- The OS keeps the exit status until the parent collects it.

Funny line:

> “The crewmate is dead, but the body is still on the ship because the parent never reported it.”

---

### 6:45–7:30 — Class Vote

Ask the class:

> “Who is the Impostor: Red, Blue, Green, Yellow, Purple, or Cyan?”

Then reveal:

```text
[EMERGENCY MEETING]
Evidence:
- PID 4217 has growing VmRSS
- PID 4217 created zombie children
- PID 4217 consumed abnormal CPU

Cyan was The Impostor.
```

---

### 7:30–8:00 — Conclusion

Presenter 1 closes:

> “This demo connects processes, scheduling, and memory management. The fun story is Among Us, but every clue came from Linux internals: PCB-visible process state, scheduler behavior, and virtual memory observation.”

---

## 9. Team Split for 3 People

### Team Member 1 — Process Detective

Responsible for:

- `fork()` / parent-child structure
- `ps`
- `pstree`
- `/proc/<pid>/status`
- Zombies
- PCB explanation

Main explanation:

> “The OS tracks every process in a kernel data structure. We cannot see the PCB directly, but Linux exposes parts of it through `ps` and `/proc`.”

---

### Team Member 2 — Scheduler Detective

Responsible for:

- CPU-bound vs sleeping processes
- `top`
- `pidstat`
- `nice` / `renice`
- Process states `R`, `S`, `T`
- Link to MLFQ and CFS

Main explanation:

> “The scheduler decides which runnable process gets CPU time. A CPU-heavy Impostor stays runnable, while normal crewmates often sleep, so the tools reveal very different behavior.”

---

### Team Member 3 — Memory Detective

Responsible for:

- Memory leak behavior
- `malloc()` and `memset()`
- `/proc/<pid>/maps`
- `VmSize`, `VmRSS`, `VmData`
- Page faults
- Optional `valgrind`

Main explanation:

> “Allocating virtual memory is not the same as using physical memory. Once the Impostor touches pages, RSS and page-fault activity reveal the sabotage.”

---

## 10. Implementation Plan

Recommended files:

```text
amongos/
├── Makefile
├── main.c
├── crew.h
├── crew.c
├── sabotage.h
├── sabotage.c
└── README.md
```

### `main.c`

Responsibilities:

- Parse command-line argument
- Start controller
- Fork child processes
- Assign names/roles
- Print PIDs clearly
- Keep parent alive
- Clean up children on exit

### `crew.c`

Responsibilities:

- Normal crewmate behavior
- Sleeping worker
- CPU worker
- File descriptor worker
- Memory worker

### `sabotage.c`

Responsibilities:

- CPU sabotage
- Memory leak sabotage
- Zombie sabotage
- Signal handling

---

## 11. Important Technical Detail: Process Names

To make `ps` output look good, set process names using `prctl()`:

```c
#include <sys/prctl.h>

prctl(PR_SET_NAME, "Crewmate_Red", 0, 0, 0);
```

This makes commands like `ps` and `top` easier to read.

Example output:

```text
PID     PPID    STAT    COMMAND
4210    4209    S       Controller
4211    4210    S       Crewmate_Red
4212    4210    R       Crewmate_Blue
4213    4210    S       Crewmate_Green
4214    4210    Z       Crewmate_Body
4215    4210    R       Impostor_Cyan
```

---

## 12. Useful Commands Cheat Sheet With Explanations and Purpose

This section explains **what each command does**, **why we use it in the demo**, and **which OS concept it proves**. During the presentation, do not just run the commands. Say the purpose out loud before or after running each one.

---

### 12.1 Start the AmongOS Program

```bash
./amongos --impostor memory
```

**What it does:**  
Starts the spaceship controller and creates several child processes. In this mode, the Impostor performs memory sabotage.

**Purpose in the demo:**  
This launches the whole live scenario. It gives you real Linux processes to investigate instead of only talking about theory.

**OS concept shown:**

- `fork()` creates child processes.
- The parent controller manages the child processes.
- Each crewmate has its own PID and process state.

**What to say:**

> “This command starts our spaceship. Every crewmate you see is not fake text; it is a real Linux process that we can inspect from the outside.”

---

```bash
./amongos --impostor cpu
```

**What it does:**  
Starts the demo with a CPU-heavy Impostor.

**Purpose in the demo:**  
Useful when you want to focus on scheduling behavior and CPU usage.

**OS concept shown:**

- CPU-bound workload
- Runnable process state
- Scheduler decisions
- CPU-share competition

**What to say:**

> “In this version, the Impostor is not leaking memory. It is sabotaging the ship by constantly demanding CPU time.”

---

```bash
./amongos --impostor zombie
```

**What it does:**  
Starts the demo with an Impostor that creates zombie child processes.

**Purpose in the demo:**  
Useful when you want a very visible process-management failure.

**OS concept shown:**

- `fork()`
- `exit()`
- Missing `wait()`
- Zombie state `Z`
- Process table entry remains after execution ends

**What to say:**

> “Here the Impostor kills crewmates but does not collect their exit status. That leaves zombie bodies in the process table.”

---

```bash
./amongos --impostor combo
```

**What it does:**  
Starts a combined sabotage mode, for example CPU abuse, memory growth, and zombie creation together.

**Purpose in the demo:**  
This is the most impressive version, but also the riskiest. It shows all three project topics in one run.

**OS concept shown:**

- Processes
- Scheduling
- Memory management
- Signals and cleanup

**What to say:**

> “This is the full chaos mode. It gives us multiple clues, so the class has to decide which process is the Impostor based on OS evidence.”

---

### 12.2 Process Tree Commands

```bash
pstree -p <controller_pid>
```

**What it does:**  
Shows the parent-child process tree starting from the spaceship controller. The `-p` option displays PIDs next to process names.

**Purpose in the demo:**  
Proves that the crewmates are real child processes created by the controller. It makes the family relationship visible.

**OS concept shown:**

- Parent process
- Child process
- Process hierarchy
- Result of `fork()`

**How to read the output:**

```text
Controller(4210)
├── Crewmate_Red(4211)
├── Crewmate_Blue(4212)
└── Impostor_Cyan(4215)
```

This means process `4210` is the parent, and the others are its children.

**What to say:**

> “This tree is our spaceship crew. The controller forked each crewmate, so the OS tracks them as parent and child processes.”

---

```bash
pstree -p
```

**What it does:**  
Shows the full system process tree.

**Purpose in the demo:**  
Useful if you lose the controller PID or want to show where your project fits inside the whole Linux process hierarchy.

**OS concept shown:**

- All processes belong to a larger tree.
- Your project is only one subtree inside the operating system.

**What to say:**

> “Our spaceship is not isolated. It is part of the full Linux process tree.”

---

### 12.3 Process State Commands

```bash
ps -o pid,ppid,stat,ni,pri,pcpu,pmem,comm -p <pid>
```

**What it does:**  
Shows selected information about one process.

Column meanings:

| Column | Meaning | Why it matters |
|---|---|---|
| `PID` | Process ID | Unique identity of the process |
| `PPID` | Parent process ID | Shows who created or manages it |
| `STAT` | Process state | Shows if it is running, sleeping, stopped, or zombie |
| `NI` | Nice value | User-level scheduling priority influence |
| `PRI` | Kernel scheduling priority | Helps discuss scheduler behavior |
| `%CPU` | CPU usage | Reveals CPU sabotage |
| `%MEM` | Memory usage | Reveals memory sabotage |
| `COMM` | Command/process name | Makes crewmates easy to identify |

**Purpose in the demo:**  
This is one of the most important commands because it connects the game characters to real PCB-visible information.

**OS concept shown:**

- PCB-related information
- Process states
- Scheduling priority
- CPU and memory usage

**What to say:**

> “We cannot directly print the kernel PCB, but `ps` shows information that comes from the kernel’s process tracking, such as PID, parent PID, state, priority, CPU use, and memory use.”

---

```bash
ps -eo pid,ppid,stat,ni,pri,pcpu,pmem,comm | grep -E "Crewmate|Impostor|Controller"
```

**What it does:**  
Lists all processes on the system, then filters the output to show only your AmongOS processes.

**Purpose in the demo:**  
Gives a clean overview of the whole crew without unrelated system processes.

**OS concept shown:**

- Multiple processes exist at the same time.
- Each has separate state, priority, CPU usage, and memory usage.
- The OS can observe and report these differences.

**What to say:**

> “This is our suspect list. We are filtering the system’s process table to show only the crewmates and the controller.”

---

```bash
ps -o pid,ppid,stat,comm
```

**What it does:**  
Shows a compact process list with PID, parent PID, state, and command name.

**Purpose in the demo:**  
Useful for quickly finding zombie state `Z`, stopped state `T`, sleeping state `S`, or running state `R`.

**OS concept shown:**

- `R` = running or runnable
- `S` = sleeping
- `T` = stopped
- `Z` = zombie

**What to say:**

> “The `STAT` column is the evidence board. It tells us what state the OS currently has recorded for each process.”

---

```bash
cat /proc/<pid>/status
```

**What it does:**  
Prints detailed kernel-provided status information for a process.

**Purpose in the demo:**  
Shows process information directly from `/proc`, not only through `ps`. This makes the observation feel closer to the OS internals.

**OS concept shown:**

- `/proc` as a window into kernel process data
- Process state
- PID and PPID
- Memory usage
- Signal masks
- Threads

**What to say:**

> “`/proc` is not a normal folder of saved text files. It is a live interface where the kernel exposes information about running processes.”

---

```bash
cat /proc/<pid>/status | grep -E "Name|State|Pid|PPid|VmSize|VmRSS|VmData|Threads"
```

**What it does:**  
Prints only the most useful lines from `/proc/<pid>/status`.

**Purpose in the demo:**  
Keeps the output short enough for an 8-minute presentation.

**OS concept shown:**

- Process identity
- Process state
- Parent-child relationship
- Virtual memory and resident memory
- Thread count

**What to say:**

> “We filter the status file so the audience can focus on the fields that connect directly to our lecture concepts.”

---

### 12.4 Scheduling and CPU Commands

```bash
top -p <pid>
```

**What it does:**  
Opens a live view of CPU and memory usage for one process.

**Purpose in the demo:**  
Shows whether a suspect is consuming abnormal CPU. This is the easiest way for the class to see CPU sabotage.

**OS concept shown:**

- CPU-bound process
- Scheduler-visible CPU usage
- Process priority and nice value
- Runtime competition

**What to say:**

> “If one crewmate is constantly near 100% CPU, that process is always asking the scheduler for CPU time. That is suspicious behavior.”

---

```bash
top -p <pid1>,<pid2>,<pid3>,<pid4>,<pid5>,<pid6>
```

**What it does:**  
Shows several chosen processes in `top` at the same time.

**Purpose in the demo:**  
Lets the class compare crewmates side by side. One CPU-heavy Impostor should stand out.

**OS concept shown:**

- CPU share comparison
- Runnable vs sleeping behavior
- Scheduler competition between processes

**What to say:**

> “This lets us compare all suspects under the same conditions. We are not guessing; we are comparing live scheduler evidence.”

---

```bash
pidstat -p <pid> 1
```

**What it does:**  
Prints CPU statistics for one process every 1 second.

**Purpose in the demo:**  
Gives clearer, line-by-line evidence than `top`, especially if you want to mention measurement over time.

**OS concept shown:**

- CPU utilization over time
- User time vs system time
- Repeated measurements instead of one snapshot

**What to say:**

> “`top` gives a live screen, but `pidstat` gives measurements over time. This is better evidence than one single observation.”

---

```bash
pidstat -p ALL 1
```

**What it does:**  
Prints CPU statistics for all processes every 1 second.

**Purpose in the demo:**  
Useful when you do not yet know which PID is suspicious.

**OS concept shown:**

- The scheduler is handling many processes at once.
- The Impostor can be detected by comparing CPU usage against the others.

**What to say:**

> “At the start of the investigation, we do not know who the Impostor is, so we measure everyone.”

---

```bash
nice -n 10 ./amongos --impostor cpu
```

**What it does:**  
Starts the program with a lower scheduling priority. Higher nice values mean the process is being nicer to others, so it usually receives less CPU under competition.

**Purpose in the demo:**  
Demonstrates how user-level priority can influence scheduling.

**OS concept shown:**

- Nice value
- Scheduling priority
- CPU-share fairness

**What to say:**

> “A higher nice value means the process is less aggressive. It is politely letting other processes have more CPU time.”

---

```bash
renice +10 -p <suspicious_pid>
```

**What it does:**  
Changes the nice value of an already-running process.

**Purpose in the demo:**  
Lets you change the Impostor’s scheduling behavior live and then show the effect in `top` or `pidstat`.

**OS concept shown:**

- Runtime priority adjustment
- Scheduler behavior can be influenced
- Difference between process behavior and scheduler policy

**What to say:**

> “We are not killing the Impostor yet. We are changing its priority and watching whether the scheduler gives it less CPU.”

---

```bash
ps -o pid,ni,pri,pcpu,comm -p <pid>
```

**What it does:**  
Shows the selected process’s nice value, priority, and CPU usage.

**Purpose in the demo:**  
Good before-and-after command when using `renice`.

**OS concept shown:**

- Nice value visible to user space
- Priority visible through tools
- CPU share after priority change

**What to say:**

> “After changing the nice value, we check whether the process priority and CPU usage reflect that change.”

---

### 12.5 Memory Commands

```bash
cat /proc/<pid>/status | grep -E "VmSize|VmRSS|VmData"
```

**What it does:**  
Shows the process’s virtual memory size, resident physical memory, and data/heap-related memory.

Column meanings:

| Field | Meaning | Why it matters |
|---|---|---|
| `VmSize` | Total virtual address space used by the process | Shows virtual memory reserved/mapped |
| `VmRSS` | Resident Set Size: physical RAM currently used | Shows real memory pressure |
| `VmData` | Data segment and heap size | Helps show heap growth from `malloc()` |

**Purpose in the demo:**  
This is the main evidence for memory sabotage. If the Impostor leaks memory, these values should grow.

**OS concept shown:**

- Virtual memory vs physical memory
- Heap growth
- Resident pages
- Demand paging

**What to say:**

> “`VmSize` is the address space the process can see. `VmRSS` is the part actually resident in RAM. The difference helps explain virtual memory.”

---

```bash
watch -n 1 'cat /proc/<pid>/status | grep -E "VmSize|VmRSS|VmData"'
```

**What it does:**  
Repeats the memory-status command every 1 second.

**Purpose in the demo:**  
Makes memory growth visible live without rerunning the command manually.

**OS concept shown:**

- Memory usage changes over time.
- A leak is not just a one-time number; it is a pattern of growth.

**What to say:**

> “A single memory reading can be misleading. Watching it over time reveals the leak pattern.”

---

```bash
cat /proc/<pid>/maps
```

**What it does:**  
Shows the memory map of a process: code, heap, shared libraries, stack, and other mapped regions.

**Purpose in the demo:**  
Makes the process address space visible. This is a direct connection to the memory-management lecture.

**OS concept shown:**

- Address-space layout
- Code segment
- Heap
- Stack
- Shared libraries
- Memory-mapped regions

**What to say:**

> “This is the process’s virtual address space. The process sees these regions as memory, but the OS maps them to physical frames using page tables.”

---

```bash
cat /proc/<pid>/maps | grep heap
```

**What it does:**  
Shows only the heap region from the memory map.

**Purpose in the demo:**  
Keeps the output short and focuses on the part affected by repeated `malloc()`.

**OS concept shown:**

- Heap region
- Dynamic memory allocation
- Relationship between `malloc()` and process address space

**What to say:**

> “The heap is where dynamic allocations usually live. If our Impostor keeps allocating memory, this is the region we expect to care about.”

---

```bash
free -h
```

**What it does:**  
Shows system-wide memory usage in human-readable units.

**Purpose in the demo:**  
Shows whether the Impostor’s memory leak is large enough to affect the whole system.

**OS concept shown:**

- Total memory
- Used memory
- Available memory
- System-level memory pressure

**What to say:**

> “`/proc/<pid>/status` shows one suspect. `free -h` shows whether the whole ship is running out of oxygen.”

---

```bash
vmstat 1
```

**What it does:**  
Prints system performance statistics every 1 second, including memory, swap, I/O, and CPU fields.

Important fields:

| Field | Meaning | Why it matters |
|---|---|---|
| `r` | Runnable processes | Shows CPU contention |
| `b` | Blocked processes | Shows processes waiting for I/O |
| `si` | Swap in | Shows pages brought from swap |
| `so` | Swap out | Shows pages moved to swap |
| `us` | User CPU time | CPU used by user programs |
| `sy` | System CPU time | CPU used by kernel work |
| `id` | Idle CPU time | Whether CPU is free |
| `wa` | I/O wait | Whether CPU waits on I/O |

**Purpose in the demo:**  
Shows whether sabotage is affecting the entire system, not only one process.

**OS concept shown:**

- System-wide memory pressure
- Swap activity
- CPU load
- Runnable queue pressure
- User vs kernel time

**What to say:**

> “`vmstat` is like the spaceship monitor. It tells us whether the problem is CPU pressure, memory pressure, swap activity, or I/O waiting.”

---

```bash
perf stat -e page-faults -p <pid>
```

**What it does:**  
Counts page faults for a specific running process until you stop the command.

**Purpose in the demo:**  
Shows that memory sabotage causes page-fault activity when new pages are touched.

**OS concept shown:**

- Page faults
- Demand paging
- Page table lookup and update
- Virtual pages becoming backed by physical frames

**What to say:**

> “When the Impostor touches new memory pages, the CPU cannot complete the access using the current mappings, so control goes to the kernel through a page fault.”

**Note:**  
`perf` may require permission on some systems. If it does not work, use `/proc/<pid>/status` and `vmstat` as the safer live-demo evidence.

---

### 12.6 File Descriptor Commands

```bash
ls -l /proc/<pid>/fd
```

**What it does:**  
Lists the open file descriptors of a process. File descriptors `0`, `1`, and `2` usually mean standard input, standard output, and standard error.

**Purpose in the demo:**  
Shows that the OS tracks process resources beyond CPU and memory. This is useful for a crewmate that opens a log file or fake task file.

**OS concept shown:**

- File descriptor table
- Per-process resources
- Kernel-managed handles
- What a process has open

**What to say:**

> “A process is not only code that runs. The OS also tracks its open files. `/proc/<pid>/fd` lets us inspect that table.”

---

```bash
readlink /proc/<pid>/fd/<fd_number>
```

**What it does:**  
Shows what a specific file descriptor points to.

**Purpose in the demo:**  
Useful if Yellow is the crewmate that opens files and you want to prove which file is open.

**OS concept shown:**

- File descriptor to file/object mapping
- Process resource tracking

**What to say:**

> “The number is only the descriptor. `readlink` shows the actual file or resource behind it.”

---

### 12.7 Signal Commands

```bash
kill -STOP <pid>
```

**What it does:**  
Stops a process. The process remains alive but does not run until continued.

**Purpose in the demo:**  
Creates a visible stopped state `T` in `ps`. It is like freezing a crewmate.

**OS concept shown:**

- Signal delivery
- Stopped process state
- OS-level process control

**What to say:**

> “This does not kill the process. It freezes it. The OS records it as stopped.”

---

```bash
kill -CONT <pid>
```

**What it does:**  
Continues a stopped process.

**Purpose in the demo:**  
Shows that a stopped process can resume execution.

**OS concept shown:**

- Signal-controlled state transition
- Stopped to runnable/sleeping again

**What to say:**

> “Now the frozen crewmate is allowed to continue. We can check `ps` again and see the state change.”

---

```bash
kill -TERM <pid>
```

**What it does:**  
Sends `SIGTERM`, asking a process to terminate gracefully. A process may handle this signal and clean up.

**Purpose in the demo:**  
Shows polite termination. Good for explaining the difference between normal shutdown and forced killing.

**OS concept shown:**

- Signal handling
- Graceful termination
- Cleanup opportunity

**What to say:**

> “`SIGTERM` is like voting someone out politely. The process can receive the signal and clean up before exiting.”

---

```bash
kill -KILL <pid>
```

**What it does:**  
Sends `SIGKILL`, which immediately kills the process. It cannot be caught, blocked, or ignored.

**Purpose in the demo:**  
Shows forced OS-level termination.

**OS concept shown:**

- Non-catchable signal
- Forced process termination
- Kernel authority over processes

**What to say:**

> “`SIGKILL` is the emergency ejection button. The process does not get to negotiate.”

---

```bash
ps -o pid,ppid,stat,comm -p <pid>
```

**What it does:**  
Checks the process state after a signal.

**Purpose in the demo:**  
Confirms that `SIGSTOP`, `SIGCONT`, `SIGTERM`, or `SIGKILL` actually changed the process state or removed the process.

**OS concept shown:**

- Observable state transitions
- Evidence-based demo, not just command running

**What to say:**

> “We do not just send a signal and assume it worked. We verify the state afterward.”

---

### 12.8 Zombie Investigation Commands

```bash
ps -o pid,ppid,stat,comm | grep Z
```

**What it does:**  
Searches the process list for zombie processes.

**Purpose in the demo:**  
Finds the “dead body” evidence.

**OS concept shown:**

- Zombie state `Z`
- Process has exited but has not been reaped by parent
- Parent did not call `wait()`

**What to say:**

> “This is the dead body report. If we see `Z`, the process is finished but still has an entry in the process table.”

---

```bash
cat /proc/<zombie_pid>/status
```

**What it does:**  
Shows status information for the zombie process.

**Purpose in the demo:**  
Proves that the zombie still exists as a process-table entry even though it is no longer executing.

**OS concept shown:**

- Minimal remaining process metadata
- Exit status waiting for parent
- PCB/process table cleanup after `wait()`

**What to say:**

> “The zombie has no normal execution left, but the kernel still keeps enough information so the parent can collect its exit status.”

---

```bash
pstree -p <controller_pid>
```

**What it does:**  
Shows where the zombie sits in the process tree.

**Purpose in the demo:**  
Connects the zombie to the parent that failed to call `wait()`.

**OS concept shown:**

- Parent responsibility
- Child cleanup
- Why zombies are usually a parent-process bug

**What to say:**

> “The zombie is evidence against the parent. The child is dead, but the parent has not collected it.”

---

### 12.9 System Call Tracing Commands

```bash
strace -f ./amongos --impostor zombie
```

**What it does:**  
Runs the program under `strace` and follows child processes with `-f`. It prints system calls such as `fork`, `clone`, `execve`, `wait4`, `kill`, `brk`, and `mmap`.

**Purpose in the demo:**  
Shows user/kernel boundary crossings. It proves that process creation, memory growth, and signals involve system calls into the kernel.

**OS concept shown:**

- Trap into kernel mode
- System calls
- Process creation API
- User mode vs kernel mode

**What to say:**

> “Our C code cannot directly create a process by editing kernel data structures. It asks the kernel through system calls, and `strace` lets us watch those requests.”

---

```bash
strace -f -e trace=clone,fork,vfork,execve,wait4 ./amongos --impostor zombie
```

**What it does:**  
Runs the program under `strace`, but filters the output to process-related system calls only.

**Purpose in the demo:**  
Cleaner than full `strace`; useful for an 8-minute presentation.

**OS concept shown:**

- `fork()`/`clone()` creates processes
- `execve()` replaces the process image
- `wait4()` reaps child processes

**What to say:**

> “This filtered trace shows the exact system calls behind our process story.”

---

```bash
strace -e brk,mmap,munmap ./amongos --impostor memory
```

**What it does:**  
Traces memory-management-related system calls.

**Purpose in the demo:**  
Shows how memory allocation may cause the process to request more address space from the kernel.

**OS concept shown:**

- `brk()` heap growth
- `mmap()` memory mapping
- `munmap()` unmapping
- Relationship between `malloc()` and kernel memory APIs

**What to say:**

> “`malloc()` is a library call, but when the allocator needs more memory, it can ask the kernel using `brk()` or `mmap()`.”

---

### 12.10 Memory Bug Diagnosis Commands

```bash
valgrind ./amongos --impostor leak-demo
```

**What it does:**  
Runs the program under Valgrind to detect memory errors and leaks.

**Purpose in the demo:**  
Useful as an optional final proof that a suspicious process has memory problems.

**OS concept shown:**

- Heap misuse
- Leaks
- Invalid reads/writes
- Use-after-free or double-free if implemented

**What to say:**

> “The OS can show us memory growth, but Valgrind helps diagnose whether our program logic is misusing heap memory.”

---

```bash
valgrind --leak-check=full ./amongos --impostor leak-demo
```

**What it does:**  
Runs Valgrind with detailed leak reporting.

**Purpose in the demo:**  
Shows exactly where leaked memory came from. This is better for Q&A than for the main 8-minute live demo because it can be slower.

**OS concept shown:**

- Heap allocation tracking
- Memory leak evidence
- Connection between C bug and observed memory growth

**What to say:**

> “This gives a deeper diagnosis: not only that memory grew, but which allocation was not freed.”

---

### 12.11 Cleanup Commands

```bash
./cleanup.sh
```

**What it does:**  
Runs your prepared cleanup script to stop any remaining AmongOS processes.

**Purpose in the demo:**  
Prevents leftover CPU loops, memory leaks, or zombies from affecting the next presentation or your laptop.

**OS concept shown:**

- Process cleanup
- Signal-based termination
- Responsible live-demo practice

**What to say:**

> “Because we created real processes, we also clean them up like real processes.”

---

```bash
pkill -f amongos
pkill -f Crewmate
pkill -f Impostor
```

**What it does:**  
Finds processes whose command line matches the given pattern and sends them a termination signal.

**Purpose in the demo:**  
Emergency cleanup if something keeps running.

**OS concept shown:**

- Matching processes by command line
- Sending signals to multiple processes
- Cleanup after controlled sabotage

**What to say:**

> “This is our emergency airlock. If a worker keeps running, we remove it before it affects the system.”

**Warning:**  
Use specific names so you do not accidentally kill unrelated processes. Test the cleanup script before presentation day.

---

### 12.12 Recommended Command Order During the Live Demo

Use this order to keep the investigation understandable:

```bash
./amongos --impostor combo
```

**Purpose:** Start the game.

```bash
pstree -p <controller_pid>
```

**Purpose:** Show the crew as a real process tree.

```bash
ps -eo pid,ppid,stat,ni,pri,pcpu,pmem,comm | grep -E "Crewmate|Impostor|Controller"
```

**Purpose:** Create the suspect list with state, CPU, memory, and priority evidence.

```bash
top -p <pid1>,<pid2>,<pid3>,<pid4>,<pid5>,<pid6>
```

**Purpose:** Look for CPU sabotage.

```bash
watch -n 1 'cat /proc/<suspect_pid>/status | grep -E "VmSize|VmRSS|VmData"'
```

**Purpose:** Look for memory sabotage over time.

```bash
cat /proc/<suspect_pid>/maps | grep heap
```

**Purpose:** Connect memory growth to the heap and address-space layout.

```bash
ps -o pid,ppid,stat,comm | grep Z
```

**Purpose:** Look for zombie bodies.

```bash
kill -STOP <suspect_pid>
ps -o pid,ppid,stat,comm -p <suspect_pid>
kill -CONT <suspect_pid>
```

**Purpose:** Demonstrate signal-controlled state transitions.

```bash
./cleanup.sh
```

**Purpose:** End safely and cleanly.

---

## 12.12A What You Are Looking For in the Output: Investigation Flow

This is the most important part of the demo. Do not run commands as a random checklist. Treat the output like evidence in an investigation.

The basic question is always:

> Which process behaves differently from the normal crewmates?

The Impostor is the process whose output looks abnormal in one or more of these ways:

| Evidence Type | Command | Suspicious Output | Meaning |
|---|---|---|---|
| Process family | `pstree -p <controller_pid>` | One process creates extra children | Possible fork/zombie sabotage |
| Process state | `ps ...` | `R` constantly, `Z`, or `T` | CPU abuse, zombie, or stopped process |
| CPU usage | `top` / `pidstat` | One process near 90–100% CPU | CPU-bound Impostor |
| Memory usage | `/proc/<pid>/status` | `VmRSS` or `VmData` keeps increasing | Memory leak Impostor |
| Address space | `/proc/<pid>/maps | grep heap` | Heap region exists/grows or is relevant | Dynamic heap allocation |
| Zombie evidence | `ps ... | grep Z` | A process with `STAT = Z` | Parent did not call `wait()` |
| Signal response | `kill -STOP`, `kill -CONT`, then `ps` | State changes to `T`, then back | OS-controlled process state transition |

---

### Full Story Flow for the Presentation

Think of the demo as six scenes.

---

### Scene 1 — Launch the Spaceship

Run:

```bash
./amongos --impostor combo
```

or safer:

```bash
./amongos --impostor memory
```

What the audience should see:

```text
[SPACESHIP] Controller PID: 4210
[RED] PID 4211 started
[BLUE] PID 4212 started
[GREEN] PID 4213 started
[YELLOW] PID 4214 started
[PURPLE] PID 4215 started
[???] One crewmate is the Impostor.
```

What you are looking for:

- The controller PID.
- The child PIDs.
- Clear names for the processes.

Why this matters:

You need the PIDs for every later command. This scene proves that the demo uses real Linux processes, not just printed text.

What to say:

> “First we launch the spaceship. The controller is the parent process. Each crewmate is a child process created with `fork()`.”

---

### Scene 2 — Build the Suspect List

Run:

```bash
pstree -p <controller_pid>
```

Expected normal output:

```text
amongos(4210)
├── Crewmate_Red(4211)
├── Crewmate_Blue(4212)
├── Crewmate_Green(4213)
├── Crewmate_Yellow(4214)
└── Crewmate_Purple(4215)
```

What you are looking for:

- All crewmates should appear under the controller.
- Their PIDs should match the program output.
- If one crewmate has extra children, that is suspicious.

Suspicious output example:

```text
amongos(4210)
├── Crewmate_Red(4211)
├── Impostor_Cyan(4216)
│   ├── Body(4217)
│   ├── Body(4218)
│   └── Body(4219)
```

What it means:

The Impostor is creating extra child processes. If those children become zombies, the parent is probably not calling `wait()`.

What to say:

> “This is our suspect list. The tree shows the parent-child relationship created by `fork()`. If one process creates unexpected children, that becomes our first clue.”

---

### Scene 3 — Check Process States

Run:

```bash
ps -eo pid,ppid,stat,ni,pri,pcpu,pmem,comm | grep -E "Crewmate|Impostor|amongos"
```

Example output:

```text
4210  3980 S   0  20   0.0  0.1 amongos
4211  4210 S   0  20   0.0  0.1 Crewmate_Red
4212  4210 R   0  20  98.7  0.1 Crewmate_Blue
4213  4210 S   0  20   0.0  5.8 Crewmate_Green
4214  4210 Z   0  20   0.0  0.0 Body
4215  4210 S   0  20   0.0  0.1 Crewmate_Purple
```

What you are looking for:

- `STAT = R`: running or runnable. Suspicious if it stays `R` and has high CPU.
- `STAT = S`: sleeping. Usually normal for waiting or I/O-like crewmates.
- `STAT = Z`: zombie. Very suspicious.
- `%CPU` near 100: CPU sabotage.
- `%MEM` increasing: possible memory sabotage.
- `PPID`: tells you which parent created the process.

How to explain the example:

- `Crewmate_Blue` is suspicious because it is `R` and uses `98.7%` CPU.
- `Crewmate_Green` is suspicious because `%MEM` is higher than the others.
- `Body` is suspicious because it has state `Z`.

What to say:

> “The `STAT`, `%CPU`, and `%MEM` columns are our evidence board. We are looking for a process that behaves differently from the others.”

---

### Scene 4 — Investigate CPU Sabotage

Run:

```bash
top -p <pid1>,<pid2>,<pid3>,<pid4>,<pid5>
```

or:

```bash
pidstat -p <suspicious_pid> 1
```

Example suspicious output:

```text
PID    %usr   %system   %CPU   Command
4212   99.0     0.0     99.0   Crewmate_Blue
```

What you are looking for:

- One process repeatedly near 90–100% CPU.
- Other crewmates near 0% or only occasionally active.
- The same process staying high across several seconds, not only one instant.

What it means:

That process is CPU-bound. It is always runnable and constantly asks the scheduler for CPU time.

What not to overclaim:

Do not say Linux is using pure MLFQ. Say this relates to the scheduling ideas from class, while Linux uses CFS.

What to say:

> “Blue is constantly runnable and consumes almost a full CPU core. This is CPU sabotage. The scheduler must keep giving CPU time to runnable processes, while sleeping crewmates do not compete as much.”

Optional live action:

```bash
renice +10 -p <suspicious_pid>
```

Then check:

```bash
ps -o pid,ni,pri,pcpu,comm -p <suspicious_pid>
```

What you are looking for after `renice`:

- `NI` changes to `10`.
- Under CPU competition, its CPU share may decrease.

Purpose:

This shows that scheduling behavior can be influenced without killing the process.

---

### Scene 5 — Investigate Memory Sabotage

Run:

```bash
watch -n 1 'cat /proc/<suspect_pid>/status | grep -E "VmSize|VmRSS|VmData"'
```

Example output over time:

```text
VmSize:   105000 kB
VmRSS:     8000 kB
VmData:   98000 kB
```

A few seconds later:

```text
VmSize:   160000 kB
VmRSS:    64000 kB
VmData:  153000 kB
```

What you are looking for:

- `VmData` increases: the heap/data area is growing.
- `VmRSS` increases: the process is using more real physical RAM.
- The numbers keep increasing every second.

What it means:

The process is probably repeatedly allocating memory and touching it. This is memory leak sabotage.

Important distinction:

- `VmSize` = virtual address space.
- `VmRSS` = actual resident physical memory.
- `VmData` = data/heap-related memory.

Then run:

```bash
cat /proc/<suspect_pid>/maps | grep heap
```

Example output:

```text
55c3f8a2c000-55c3f8b91000 rw-p 00000000 00:00 0 [heap]
```

What you are looking for:

- A `[heap]` region.
- This connects the suspicious memory growth to dynamic allocation.

What to say:

> “The Impostor is not just reserving virtual addresses. Because the program touches the allocated memory with `memset`, RSS grows too. That means physical memory pages are being committed.”

---

### Scene 6 — Investigate Zombie Bodies

Run:

```bash
ps -o pid,ppid,stat,comm | grep Z
```

Example output:

```text
4217  4216 Z  Body
4218  4216 Z  Body
```

What you are looking for:

- `STAT` contains `Z`.
- The `PPID` points to the parent process responsible for the zombies.

What it means:

The child process has exited, but its parent did not call `wait()`. The OS keeps a minimal entry so the parent can collect the exit status.

Then run:

```bash
pstree -p <controller_pid>
```

What you are looking for:

- Which process is the parent of the zombie children.
- That parent becomes very suspicious.

What to say:

> “The body is dead, but still visible. In OS terms, the child exited, but the parent did not reap it using `wait()`, so the process table still contains a zombie entry.”

---

### Scene 7 — Emergency Meeting and Reveal

Summarize evidence like this:

```text
Emergency Meeting Evidence:
- Blue: 98% CPU for multiple seconds
- Green: VmRSS increased from 8 MB to 64 MB
- Cyan: created zombie children with STAT=Z
```

Then ask the class:

> “Based on the OS evidence, who is the Impostor?”

Reveal:

```text
Cyan was The Impostor.
```

What to say after the reveal:

> “We caught the Impostor by connecting Linux observations to OS internals: process hierarchy, PCB-visible state, scheduling behavior, and virtual memory.”

---

### Scene 8 — Cleanup

Run:

```bash
./cleanup.sh
```

Then verify:

```bash
ps -eo pid,ppid,stat,comm | grep -E "Crewmate|Impostor|amongos"
```

What you are looking for:

- No remaining AmongOS processes.
- No leftover CPU loops.
- No zombie bodies.

What to say:

> “Because these were real processes, we clean them up at the end. A reliable live demo should leave the system stable.”

---

### The Core Logic in One Sentence

You are not trying to understand every line of every command. You are looking for abnormal differences:

- High `%CPU` means possible CPU sabotage.
- Growing `VmRSS` / `VmData` means possible memory sabotage.
- `STAT = Z` means zombie sabotage.
- Extra children in `pstree` means suspicious process creation.
- State changes after `kill -STOP` / `kill -CONT` prove signal-controlled OS state transitions.

---

### Best Flow for an 8-Minute Demo

Use only these commands live:

```bash
./amongos --impostor combo
pstree -p <controller_pid>
ps -eo pid,ppid,stat,ni,pri,pcpu,pmem,comm | grep -E "Crewmate|Impostor|amongos"
top -p <pid1>,<pid2>,<pid3>,<pid4>,<pid5>
watch -n 1 'cat /proc/<suspect_pid>/status | grep -E "VmSize|VmRSS|VmData"'
ps -o pid,ppid,stat,comm | grep Z
./cleanup.sh
```

Use this decision path:

1. `pstree` answers: “Who exists and who created whom?”
2. `ps` answers: “What state is everyone in?”
3. `top` answers: “Who is abusing CPU?”
4. `/proc/<pid>/status` answers: “Who is growing in memory?”
5. `grep Z` answers: “Are there dead bodies?”
6. `cleanup.sh` ends the experiment safely.

This keeps the class from getting lost because every command answers one investigation question.

---

### 12.13 Commands to Avoid During the Main 8-Minute Demo

These commands are useful, but they can produce too much output or take too long. Keep them for Q&A or backup evidence.

```bash
cat /proc/<pid>/maps
```

**Reason to avoid live:**  
The full memory map is long. Use `grep heap` during the main demo.

```bash
strace -f ./amongos --impostor combo
```

**Reason to avoid live:**  
Full `strace` output can flood the terminal. Use filtered `strace` instead.

```bash
valgrind --leak-check=full ./amongos --impostor leak-demo
```

**Reason to avoid live:**  
Valgrind is slower and may not fit the timebox. It is excellent for backup or Q&A.

```bash
perf stat -e page-faults -p <pid>
```

**Reason to be careful:**  
`perf` permissions may differ between machines. Test it before presentation day.

---

## 13. Safety and Live Demo Reliability

Because the presentation must be live, reliability matters more than maximum complexity.

### Avoid dangerous memory sizes

Do not allocate unlimited memory. Use a maximum cap.

Example:

```c
#define MAX_MB 512
```

This is enough to show growth without crashing the machine.

### Use deterministic mode for the presentation

Instead of:

```bash
./amongos --impostor random
```

Use:

```bash
./amongos --impostor memory
```

or:

```bash
./amongos --impostor combo
```

This prevents surprises.

### Always have a cleanup command

Create a script:

```bash
./cleanup.sh
```

Containing:

```bash
pkill -f amongos
pkill -f Crewmate
pkill -f Impostor
```

### Practice with the same laptop

Memory and CPU behavior can vary across machines.

Practice the demo on the exact machine used for presentation.

---

## 14. Expected Professor Questions and Strong Answers

### Q1. Is this really different from just running `top`?

**Answer:**

Yes. `top` is only one observation tool. Our program creates controlled workloads where each behavior maps to a specific OS concept. We then verify the behavior using `ps`, `/proc`, `pstree`, `vmstat`, and optionally `perf` or `valgrind`.

---

### Q2. Where is the PCB?

**Answer:**

The PCB is a kernel data structure, so we cannot directly print it from user space. But Linux exposes selected PCB-related information through `/proc/<pid>/status` and tools like `ps`, including PID, PPID, state, priority, and memory statistics.

---

### Q3. Why does a zombie still appear in `ps`?

**Answer:**

The process has finished running, but the kernel keeps a minimal process table entry so the parent can collect the exit status using `wait()`. Once the parent waits, the zombie disappears.

---

### Q4. Why does `malloc()` not always immediately increase physical memory?

**Answer:**

`malloc()` may reserve virtual address space, but physical memory is usually committed when the process actually touches the pages. That is why `memset()` causes RSS and page-fault behavior to become visible.

---

### Q5. How does this connect to scheduling?

**Answer:**

CPU-bound processes remain runnable and compete for CPU time, while sleeping or I/O-like processes frequently leave the runnable queue. This lets us observe different scheduler behavior and relate it to response time, turnaround, MLFQ ideas, and Linux CFS.

---

## 15. Rubric Verification

### Complexity and Ambition

This project should score strongly because it integrates processes, scheduling, and memory management into one coherent scenario.

### Conceptual Understanding

The presenters can explain the mechanism behind every observation: PCB-visible state, forked children, zombies, signals, scheduler behavior, memory growth, and page faults.

### Soundness of Approach

The evidence is observable using standard Linux tools. The same behavior can be repeated with deterministic modes.

### Presentation, Communication, and Teamwork

The game structure creates a natural split between three presenters: process detective, scheduler detective, and memory detective.

---

## 16. Agent-Style Verification

### Agent 1: Rubric Checker

**Verdict:** Strong fit.

Reason:

- Covers all three topic areas.
- Does not feel like three unrelated demos.
- Uses live tools and observable evidence.
- Gives each team member a clear role.

Risk:

- If the story becomes too playful and the OS explanation is weak, the professor may see it as entertainment rather than depth.

Fix:

- After every funny event, immediately connect it to a precise OS concept.

---

### Agent 2: Live Demo Engineer

**Verdict:** Feasible, but should be deterministic.

Reason:

- CPU loops, memory leaks, zombies, and signals are easy to implement in C.
- The demo can be run live in a terminal.
- The tools are standard Linux tools.

Risk:

- True randomness can waste time.
- Too much memory allocation can crash or freeze the machine.

Fix:

- Use deterministic sabotage mode.
- Cap memory allocation.
- Prepare a cleanup script.
- Print all relevant PIDs clearly.

---

### Agent 3: OS Depth Reviewer

**Verdict:** Technically strong if explanations are precise.

Reason:

- Process tree demonstrates `fork()` and parent-child relations.
- Zombie mode demonstrates missing `wait()` and process table entries.
- CPU mode demonstrates scheduling and runnable processes.
- Memory mode demonstrates heap growth, virtual memory, RSS, and page faults.
- Signal mode demonstrates process control and state transitions.

Risk:

- Mentioning MLFQ too strongly may be inaccurate because Linux uses CFS, not a pure MLFQ scheduler.

Fix:

- Say: “This behavior relates to the scheduling trade-offs we studied with MLFQ, while Linux itself uses CFS.”

---

## 17. Final Recommendation

Build **Among Us: The Impostor Process** as the final project.

For the actual presentation, use this exact structure:

1. Start the spaceship controller.
2. Show the process tree.
3. Investigate CPU behavior.
4. Investigate memory behavior.
5. Reveal a zombie body.
6. Let the class vote.
7. Reveal the Impostor.
8. End by mapping every game event back to an OS concept.

This is fun enough to stand out, but still serious enough to satisfy the assignment requirements.

---

## 18. Minimal Build Target

If time is short, implement only:

- Parent controller
- Five child processes
- One CPU-heavy process
- One memory-leaking process
- One zombie-creating process
- Process names using `prctl()`
- Cleanup script
- README with demo commands

That is already enough for a strong live demonstration.

---

## 19. Stretch Features

Add these only after the core demo works:

- Terminal colors
- ASCII emergency meeting screen
- Random Impostor selection
- `--impostor cpu`, `--impostor memory`, `--impostor zombie`, `--impostor combo`
- Automatic printing of useful investigation commands
- Signal handler that prints when a crewmate receives `SIGTERM`
- `strace` mode to show system calls
- Valgrind demo mode with a deliberate memory bug

---

## 20. Final Tagline

> “Among Us is the theme. Linux internals are the evidence.”

