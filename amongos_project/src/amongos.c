#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_CREW 6
#define MAX_ALLOCS 512
#define DEFAULT_MEM_STEP_MB 8
#define DEFAULT_MEM_LIMIT_MB 256
#define DEFAULT_ZOMBIE_LIMIT 5

static volatile sig_atomic_t keep_running = 1;
static pid_t children[MAX_CREW];
static int child_count = 0;

static const char *colors[MAX_CREW] = {"red", "blue", "green", "yellow", "purple", "cyan"};

typedef enum {
    ROLE_SLEEPER,
    ROLE_CPU,
    ROLE_MEMORY,
    ROLE_FD,
    ROLE_NORMAL,
    ROLE_ZOMBIE,
    ROLE_COMBO
} role_t;

typedef struct {
    const char *sabotage;
    int duration;
    int mem_step_mb;
    int mem_limit_mb;
    int zombie_limit;
    bool reveal;
} config_t;

static void on_signal(int sig) {
    (void)sig;
    keep_running = 0;
}

static void say(const char *color, const char *fmt, ...) {
    va_list args;
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char ts[16];
    strftime(ts, sizeof(ts), "%H:%M:%S", tm);
    printf("[%s] %-6s PID=%d PPID=%d | ", ts, color, getpid(), getppid());
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
    fflush(stdout);
}

static void set_proc_name(const char *color) {
    char name[16];
    snprintf(name, sizeof(name), "among_%s", color);
    prctl(PR_SET_NAME, name, 0, 0, 0);
}

static void sleeper_worker(const char *color) {
    set_proc_name(color);
    while (keep_running) {
        say(color, "doing normal spaceship task, then sleeping (STAT should often be S)");
        sleep(3);
    }
}

static void normal_worker(const char *color) {
    set_proc_name(color);
    unsigned long tick = 0;
    while (keep_running) {
        tick++;
        say(color, "normal crewmate heartbeat %lu", tick);
        sleep(4);
    }
}

static void cpu_worker(const char *color) {
    set_proc_name(color);
    say(color, "CPU sabotage started: this process should stay RUNNABLE and show high %%CPU");
    volatile double x = 1.000001;
    unsigned long rounds = 0;
    while (keep_running) {
        for (int i = 0; i < 10000000; i++) {
            x = x * 1.0000001 + 0.0000003;
            if (x > 1000000.0) x = 1.000001;
        }
        rounds++;
        if (rounds % 40 == 0) {
            say(color, "still burning CPU; look for high PCPU in ps/top/pidstat");
        }
    }
}

static void memory_worker(const char *color, int step_mb, int limit_mb) {
    set_proc_name(color);
    say(color, "memory sabotage started: VmRSS/VmData should grow until %d MB cap", limit_mb);
    void *blocks[MAX_ALLOCS];
    int count = 0;
    int used_mb = 0;
    memset(blocks, 0, sizeof(blocks));
    while (keep_running && count < MAX_ALLOCS && used_mb + step_mb <= limit_mb) {
        size_t bytes = (size_t)step_mb * 1024UL * 1024UL;
        char *p = malloc(bytes);
        if (!p) {
            say(color, "malloc failed after about %d MB", used_mb);
            break;
        }
        for (size_t i = 0; i < bytes; i += 4096) {
            p[i] = (char)(count + 1); // touch each page so RSS really grows
        }
        blocks[count++] = p;
        used_mb += step_mb;
        say(color, "leaked %d MB total; check /proc/%d/status for VmRSS/VmData", used_mb, getpid());
        sleep(2);
    }
    say(color, "memory cap reached; keeping leaked blocks allocated so evidence remains visible");
    while (keep_running) sleep(5);
    // intentionally free on shutdown so normal cleanup is not dirty
    for (int i = 0; i < count; i++) free(blocks[i]);
}

static void fd_worker(const char *color) {
    set_proc_name(color);
    int fds[4];
    for (int i = 0; i < 4; i++) {
        fds[i] = open("/dev/null", O_RDONLY);
    }
    say(color, "opened several file descriptors; inspect /proc/%d/fd", getpid());
    while (keep_running) sleep(5);
    for (int i = 0; i < 4; i++) if (fds[i] >= 0) close(fds[i]);
}

static void zombie_worker(const char *color, int zombie_limit) {
    set_proc_name(color);
    say(color, "zombie sabotage started: I will fork children that exit, without wait()");
    int made = 0;
    while (keep_running && made < zombie_limit) {
        pid_t pid = fork();
        if (pid == 0) {
            prctl(PR_SET_NAME, "among_body", 0, 0, 0);
            _exit(42);
        } else if (pid > 0) {
            made++;
            say(color, "created zombie body child PID=%d; look for STAT=Z with ps", pid);
        } else {
            say(color, "fork failed: %s", strerror(errno));
        }
        sleep(4);
    }
    say(color, "zombie cap reached; not calling wait(), so bodies remain until I die");
    while (keep_running) sleep(5);
}

static void combo_worker(const char *color, int mem_step_mb, int mem_limit_mb, int zombie_limit) {
    set_proc_name(color);
    say(color, "COMBO sabotage: CPU burn + memory leak + zombie bodies");
    void *blocks[MAX_ALLOCS];
    int count = 0, used_mb = 0, zombies = 0;
    memset(blocks, 0, sizeof(blocks));
    volatile double x = 1.1;
    int loop = 0;
    while (keep_running) {
        for (int i = 0; i < 8000000; i++) {
            x = x * 1.0000001 + 0.0000001;
            if (x > 100000.0) x = 1.1;
        }
        loop++;
        if (loop % 15 == 0 && used_mb + mem_step_mb <= mem_limit_mb && count < MAX_ALLOCS) {
            size_t bytes = (size_t)mem_step_mb * 1024UL * 1024UL;
            char *p = malloc(bytes);
            if (p) {
                for (size_t i = 0; i < bytes; i += 4096) p[i] = (char)(count + 1);
                blocks[count++] = p;
                used_mb += mem_step_mb;
                say(color, "combo evidence: CPU high + leaked %d MB", used_mb);
            }
        }
        if (loop % 30 == 0 && zombies < zombie_limit) {
            pid_t pid = fork();
            if (pid == 0) {
                prctl(PR_SET_NAME, "among_body", 0, 0, 0);
                _exit(99);
            } else if (pid > 0) {
                zombies++;
                say(color, "combo evidence: created zombie body PID=%d", pid);
            }
        }
    }
    for (int i = 0; i < count; i++) free(blocks[i]);
}

static role_t role_for_index(int index, const char *sabotage) {
    // Five normal baselines + one impostor. Cyan is the predictable impostor for live demo.
    if (index == 5) {
        if (strcmp(sabotage, "cpu") == 0) return ROLE_CPU;
        if (strcmp(sabotage, "memory") == 0) return ROLE_MEMORY;
        if (strcmp(sabotage, "zombie") == 0) return ROLE_ZOMBIE;
        if (strcmp(sabotage, "combo") == 0) return ROLE_COMBO;
        return ROLE_COMBO;
    }
    switch (index) {
        case 0: return ROLE_SLEEPER;
        case 1: return ROLE_NORMAL;
        case 2: return ROLE_FD;
        case 3: return ROLE_SLEEPER;
        case 4: return ROLE_NORMAL;
        default: return ROLE_NORMAL;
    }
}

static const char *role_name(role_t role) {
    switch (role) {
        case ROLE_SLEEPER: return "normal sleeper";
        case ROLE_CPU: return "CPU sabotage";
        case ROLE_MEMORY: return "memory sabotage";
        case ROLE_FD: return "file descriptor baseline";
        case ROLE_NORMAL: return "normal heartbeat";
        case ROLE_ZOMBIE: return "zombie sabotage";
        case ROLE_COMBO: return "COMBO impostor sabotage";
        default: return "unknown";
    }
}

static void child_main(const char *color, role_t role, const config_t *cfg) {
    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);
    switch (role) {
        case ROLE_SLEEPER: sleeper_worker(color); break;
        case ROLE_NORMAL: normal_worker(color); break;
        case ROLE_FD: fd_worker(color); break;
        case ROLE_CPU: cpu_worker(color); break;
        case ROLE_MEMORY: memory_worker(color, cfg->mem_step_mb, cfg->mem_limit_mb); break;
        case ROLE_ZOMBIE: zombie_worker(color, cfg->zombie_limit); break;
        case ROLE_COMBO: combo_worker(color, cfg->mem_step_mb, cfg->mem_limit_mb, cfg->zombie_limit); break;
    }
    say(color, "exiting");
    _exit(0);
}

static void print_usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  --sabotage cpu|memory|zombie|combo   Impostor behavior (default: combo)\n");
    printf("  --duration SECONDS                   Auto-stop after N seconds (default: 0 = manual Ctrl+C)\n");
    printf("  --mem-step-mb N                      MB allocated per memory step (default: %d)\n", DEFAULT_MEM_STEP_MB);
    printf("  --mem-limit-mb N                     Memory cap for safety (default: %d)\n", DEFAULT_MEM_LIMIT_MB);
    printf("  --zombie-limit N                     Max zombie bodies to create (default: %d)\n", DEFAULT_ZOMBIE_LIMIT);
    printf("  --reveal                             Print the impostor role at startup\n");
    printf("  --help                               Show this help\n");
}

static config_t parse_args(int argc, char **argv) {
    config_t cfg = {"combo", 0, DEFAULT_MEM_STEP_MB, DEFAULT_MEM_LIMIT_MB, DEFAULT_ZOMBIE_LIMIT, false};
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--sabotage") == 0 && i + 1 < argc) cfg.sabotage = argv[++i];
        else if (strcmp(argv[i], "--duration") == 0 && i + 1 < argc) cfg.duration = atoi(argv[++i]);
        else if (strcmp(argv[i], "--mem-step-mb") == 0 && i + 1 < argc) cfg.mem_step_mb = atoi(argv[++i]);
        else if (strcmp(argv[i], "--mem-limit-mb") == 0 && i + 1 < argc) cfg.mem_limit_mb = atoi(argv[++i]);
        else if (strcmp(argv[i], "--zombie-limit") == 0 && i + 1 < argc) cfg.zombie_limit = atoi(argv[++i]);
        else if (strcmp(argv[i], "--reveal") == 0) cfg.reveal = true;
        else if (strcmp(argv[i], "--help") == 0) { print_usage(argv[0]); exit(0); }
        else { fprintf(stderr, "Unknown or incomplete option: %s\n", argv[i]); print_usage(argv[0]); exit(2); }
    }
    if (cfg.mem_step_mb <= 0) cfg.mem_step_mb = DEFAULT_MEM_STEP_MB;
    if (cfg.mem_limit_mb < cfg.mem_step_mb) cfg.mem_limit_mb = cfg.mem_step_mb;
    if (cfg.zombie_limit < 0) cfg.zombie_limit = 0;
    return cfg;
}

static void cleanup_children(void) {
    printf("\n[SPACESHIP] Cleaning up child processes...\n");
    for (int i = 0; i < child_count; i++) {
        if (children[i] > 0) kill(children[i], SIGTERM);
    }
    sleep(1);
    for (int i = 0; i < child_count; i++) {
        if (children[i] > 0) kill(children[i], SIGKILL);
    }
    while (waitpid(-1, NULL, WNOHANG) > 0) {}
}

int main(int argc, char **argv) {
    config_t cfg = parse_args(argc, argv);
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    prctl(PR_SET_NAME, "amongos_ctrl", 0, 0, 0);

    FILE *state = fopen(".amongos_state.txt", "w");
    FILE *pidfile = fopen(".amongos_controller.pid", "w");
    if (pidfile) { fprintf(pidfile, "%d\n", getpid()); fclose(pidfile); }

    printf("\n=== AMONG OS: THE IMPOSTOR PROCESS ===\n");
    printf("Controller PID: %d\n", getpid());
    printf("Sabotage mode: %s\n", cfg.sabotage);
    printf("Safety caps: memory step=%d MB, memory limit=%d MB, zombie limit=%d\n", cfg.mem_step_mb, cfg.mem_limit_mb, cfg.zombie_limit);
    printf("The predictable hidden impostor for the live demo is CYAN. Do not reveal this until the end.\n");
    if (cfg.reveal) printf("REVEAL: cyan is running %s\n", cfg.sabotage);
    printf("State file: .amongos_state.txt\n\n");

    if (state) {
        fprintf(state, "controller %d\n", getpid());
        fprintf(state, "# color pid role\n");
    }

    for (int i = 0; i < MAX_CREW; i++) {
        role_t role = role_for_index(i, cfg.sabotage);
        pid_t pid = fork();
        if (pid == 0) {
            if (state) fclose(state);
            child_main(colors[i], role, &cfg);
        } else if (pid > 0) {
            children[child_count++] = pid;
            printf("%-6s PID=%d role=%s%s\n", colors[i], pid, role_name(role), i == 5 ? "  <-- secret impostor for presenter" : "");
            if (state) fprintf(state, "%s %d %s\n", colors[i], pid, role_name(role));
        } else {
            fprintf(stderr, "fork failed for %s: %s\n", colors[i], strerror(errno));
        }
    }
    if (state) fclose(state);

    printf("\nStart investigating from another terminal:\n");
    printf("  bash scripts/observe.sh\n");
    printf("  pstree -p %d\n", getpid());
    printf("  ps -eo pid,ppid,stat,ni,pri,pcpu,pmem,comm | grep -E 'among_'\n\n");

    time_t start = time(NULL);
    while (keep_running) {
        if (cfg.duration > 0 && time(NULL) - start >= cfg.duration) break;
        sleep(1);
        while (waitpid(-1, NULL, WNOHANG) > 0) {}
    }

    cleanup_children();
    printf("[SPACESHIP] Demo ended. Run scripts/cleanup.sh if anything remains.\n");
    return 0;
}
