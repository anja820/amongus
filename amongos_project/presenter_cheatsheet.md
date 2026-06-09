# Presenter Cheatsheet

## One-line pitch

We built a mini Among Us game where each crewmate is a real Linux process. One process is secretly sabotaging the system, and we catch it using process, scheduling, and memory evidence.

---

## Team split for 3 people

### Presenter 1: Process detective

Commands:

```bash
pstree -p <controller_pid>
ps -eo pid,ppid,stat,comm | grep -E 'among_'
ps -eo pid,ppid,stat,comm | grep -E 'Z|among_body'
```

Concepts:

- `fork()` creates children.
- PCB stores PID, PPID, state.
- Zombie = exited child not collected by `wait()`.

Main line:

> The zombie body exists because the child exited, but the parent did not call `wait()`, so the kernel keeps its exit status.

---

### Presenter 2: Scheduler detective

Commands:

```bash
ps -eo pid,ppid,stat,ni,pri,pcpu,pmem,comm | grep -E 'among_'
top -p <pid1>,<pid2>,<pid3>,<pid4>,<pid5>,<pid6>
pidstat -p <cyan_pid> 1
```

Concepts:

- CPU-bound process stays runnable.
- Scheduler decides who gets CPU time.
- High `%CPU` means repeated CPU usage, not just existence.

Main line:

> Cyan is suspicious because it is always ready to run and consumes much more CPU than the normal crewmates.

---

### Presenter 3: Memory detective

Commands:

```bash
cat /proc/<cyan_pid>/status | grep -E 'State|VmSize|VmRSS|VmData'
cat /proc/<cyan_pid>/maps | less
vmstat 1
```

Concepts:

- `malloc()` grows heap/virtual memory.
- Touching pages increases RSS.
- `/proc` exposes process memory information.

Main line:

> VmRSS grows because the Impostor touches newly allocated memory pages, forcing real physical pages to be assigned.

---

## Emergency meeting script

Say:

```text
Emergency meeting.
Evidence 1: Cyan used almost all CPU.
Evidence 2: Cyan's VmRSS and VmData increased over time.
Evidence 3: Zombie body processes appeared under Cyan.
Conclusion: Cyan is the Impostor.
```

Then run:

```bash
cat .amongos_state.txt
```

---

## Questions the professor may ask

### Why does a zombie still appear if it is dead?

Because the process has exited, but the parent has not called `wait()`. The kernel keeps a small process table entry so the parent can read the exit status.

### Why does memory not always increase immediately after `malloc()`?

Linux uses virtual memory and demand paging. Allocating may reserve virtual address space, but physical memory is usually committed when the program touches the pages.

### Why is CPU usage high for one process?

The CPU sabotage process runs a tight computation loop. It rarely blocks, so it remains runnable and competes for CPU time constantly.

### What does `SIGSTOP` prove?

It proves the OS controls process execution state. After `SIGSTOP`, `ps` shows the process in stopped state `T`. `SIGCONT` allows it to run again.

### Is this just a game or real OS behavior?

The story is a game, but every character is a real Linux process and every clue comes from real kernel-exposed data.
