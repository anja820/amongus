#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* ── limits ──────────────────────────────────────────────────────────────── */
#define MAX_CREW          6
#define MAX_ALLOCS      512
#define DEFAULT_MEM_STEP_MB   8
#define DEFAULT_MEM_LIMIT_MB 256
#define DEFAULT_ZOMBIE_LIMIT   5

/* ── ANSI colour codes ───────────────────────────────────────────────────── */
#define A_RED    "\033[1;31m"
#define A_BLUE   "\033[1;34m"
#define A_GREEN  "\033[1;32m"
#define A_YELLOW "\033[1;33m"
#define A_PURPLE "\033[1;35m"
#define A_CYAN   "\033[1;36m"
#define A_WHITE  "\033[1;37m"
#define A_BOLD   "\033[1m"
#define A_DIM    "\033[2m"
#define A_RED_BG "\033[1;41;37m"
#define A_RESET  "\033[0m"

/* ── global state ────────────────────────────────────────────────────────── */
static volatile sig_atomic_t keep_running   = 1;
static volatile sig_atomic_t do_meeting     = 0;  /* set by SIGUSR1 */
static volatile sig_atomic_t do_reveal      = 0;  /* set by SIGUSR2 */

static pid_t    children[MAX_CREW];
static int      child_count = 0;
static pid_t    ctrl_pid;

/* crew colours — index matches children[] */
static const char *crew_colors[MAX_CREW] = {
    "red", "blue", "green", "yellow", "purple", "cyan"
};

/* ── role enum ───────────────────────────────────────────────────────────── */
typedef enum {
    ROLE_SLEEPER,
    ROLE_NORMAL,
    ROLE_CPU,
    ROLE_MEMORY,
    ROLE_FD,
    ROLE_ZOMBIE,
    ROLE_ORPHAN,
    ROLE_TLB,
    ROLE_COMBO
} role_t;

/* ── config ──────────────────────────────────────────────────────────────── */
typedef struct {
    const char *sabotage;
    int  duration;
    int  mem_step_mb;
    int  mem_limit_mb;
    int  zombie_limit;
    bool reveal;
    bool no_color;
} config_t;

static config_t g_cfg;                  /* populated in main, read by workers */
static char     g_role_names[MAX_CREW][64]; /* "color: role" strings for meeting */

/* ── helpers ─────────────────────────────────────────────────────────────── */
static const char *ansi_for(const char *c) {
    if (!g_cfg.no_color) {
        if (!strcmp(c,"red"))    return A_RED;
        if (!strcmp(c,"blue"))   return A_BLUE;
        if (!strcmp(c,"green"))  return A_GREEN;
        if (!strcmp(c,"yellow")) return A_YELLOW;
        if (!strcmp(c,"purple")) return A_PURPLE;
        if (!strcmp(c,"cyan"))   return A_CYAN;
        if (!strcmp(c,"orphan")) return A_WHITE;
    }
    return "";
}

static void say(const char *color, const char *fmt, ...) {
    va_list ap;
    time_t  now = time(NULL);
    struct tm *tm = localtime(&now);
    char ts[16];
    strftime(ts, sizeof(ts), "%H:%M:%S", tm);
    const char *ac = ansi_for(color);
    printf("%s[%s] %-6s%s PID=%-6d PPID=%-6d | ",
           ac, ts, color, A_RESET, getpid(), getppid());
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    putchar('\n');
    fflush(stdout);
}

static void set_proc_name(const char *color) {
    char name[16];
    snprintf(name, sizeof(name), "among_%s", color);
    prctl(PR_SET_NAME, name, 0, 0, 0);
}

/* ── signal handlers ─────────────────────────────────────────────────────── */
static void on_signal(int sig)  { (void)sig; keep_running = 0; }
static void on_sigusr1(int sig) { (void)sig; do_meeting = 1; }
static void on_sigusr2(int sig) { (void)sig; do_reveal  = 1; }

/* ── worker: sleeper (STAT=S, I/O-like) ─────────────────────────────────── */
static void sleeper_worker(const char *color) {
    set_proc_name(color);
    while (keep_running) {
        say(color, "doing spaceship task, sleeping 3 s  (STAT should be S)");
        sleep(3);
    }
}

/* ── worker: normal heartbeat ────────────────────────────────────────────── */
static void normal_worker(const char *color) {
    set_proc_name(color);
    unsigned long tick = 0;
    while (keep_running) {
        say(color, "heartbeat %lu  (STAT=S between prints)", ++tick);
        sleep(4);
    }
}

/* ── worker: CPU sabotage (STAT=R, ~100 %CPU) ───────────────────────────── */
static void cpu_worker(const char *color) {
    set_proc_name(color);
    say(color, A_BOLD "CPU SABOTAGE" A_RESET
               " – burning CPU; process stays RUNNABLE (R) and shows high %%CPU in top/pidstat");
    volatile double x = 1.000001;
    unsigned long rounds = 0;
    while (keep_running) {
        for (int i = 0; i < 10000000; i++) {
            x = x * 1.0000001 + 0.0000003;
            if (x > 1000000.0) x = 1.000001;
        }
        if (++rounds % 40 == 0)
            say(color, "still burning CPU – compare %%CPU in: "
                       A_DIM "ps -eo pid,stat,pcpu,comm | grep among_" A_RESET);
    }
}

/* ── worker: memory leak (VmRSS grows) ──────────────────────────────────── */
static void memory_worker(const char *color, int step_mb, int limit_mb) {
    set_proc_name(color);
    say(color, A_BOLD "MEMORY SABOTAGE" A_RESET
               " – VmRSS/VmData will grow until %d MB cap", limit_mb);
    void *blocks[MAX_ALLOCS];
    int count = 0, used_mb = 0;
    memset(blocks, 0, sizeof(blocks));

    while (keep_running && count < MAX_ALLOCS && used_mb + step_mb <= limit_mb) {
        size_t bytes = (size_t)step_mb * 1024UL * 1024UL;
        char  *p     = malloc(bytes);
        if (!p) { say(color, "malloc failed after %d MB", used_mb); break; }
        /* touch every page so RSS actually grows (demand paging) */
        for (size_t i = 0; i < bytes; i += 4096) p[i] = (char)(count + 1);
        blocks[count++] = p;
        used_mb += step_mb;
        say(color, "leaked %d MB  – " A_DIM
                   "watch -n1 'grep -E VmRSS\\|VmData /proc/%d/status'" A_RESET,
                   used_mb, getpid());
        sleep(2);
    }
    say(color, "cap reached; blocks kept allocated so evidence remains");
    while (keep_running) sleep(5);
    for (int i = 0; i < count; i++) free(blocks[i]);
}

/* ── worker: file-descriptor baseline ───────────────────────────────────── */
static void fd_worker(const char *color) {
    set_proc_name(color);
    int fds[4];
    for (int i = 0; i < 4; i++) fds[i] = open("/dev/null", O_RDONLY);
    say(color, "opened 4 extra fds – inspect: " A_DIM "ls -l /proc/%d/fd" A_RESET, getpid());
    while (keep_running) sleep(5);
    for (int i = 0; i < 4; i++) if (fds[i] >= 0) close(fds[i]);
}

/* ── worker: zombie factory (missing wait) ───────────────────────────────── */
static void zombie_worker(const char *color, int limit) {
    set_proc_name(color);
    say(color, A_BOLD "ZOMBIE SABOTAGE" A_RESET
               " – forking children, never calling wait()");
    int made = 0;
    while (keep_running && made < limit) {
        pid_t pid = fork();
        if (pid == 0) {
            prctl(PR_SET_NAME, "among_body", 0, 0, 0);
            _exit(42);
        } else if (pid > 0) {
            made++;
            say(color, "zombie body PID=%d created – " A_DIM
                       "ps -o pid,ppid,stat,comm | grep Z" A_RESET, pid);
        } else {
            say(color, "fork failed: %s", strerror(errno));
        }
        sleep(4);
    }
    say(color, "zombie cap reached; parent alive but never calls wait()");
    while (keep_running) sleep(5);
}

/* ── worker: orphan demo ─────────────────────────────────────────────────── */
/*
 * Forks two grandchildren and then exits without calling wait().
 * The grandchildren are reparented to PID 1 (or the subreaper).
 * Demonstrates orphan adoption — check their PPid in /proc/<pid>/status.
 */
static void orphan_worker(const char *color) {
    set_proc_name(color);
    say(color, A_BOLD "ORPHAN SABOTAGE" A_RESET
               " – forking 2 grandchildren then exiting; watch their PPID change");

    for (int i = 0; i < 2; i++) {
        pid_t gchild = fork();
        if (gchild == 0) {
            /* grandchild: ignore SIGHUP so we survive parent's death */
            signal(SIGHUP, SIG_IGN);
            signal(SIGTERM, SIG_DFL);
            signal(SIGINT,  SIG_DFL);
            prctl(PR_SET_NAME, "among_orphan", 0, 0, 0);
            say("orphan", "orphan %d alive – my parent will exit; "
                          "watch PPID change: " A_DIM
                          "watch -n1 'grep PPid /proc/%d/status'" A_RESET,
                i + 1, getpid());
            sleep(90);
            _exit(0);
        }
    }
    say(color, "exiting now – children become orphans, reparented to PID 1");
    _exit(0);   /* exit without wait(); grandchildren are now orphaned */
}

/* ── worker: TLB thrash (sequential vs random page access) ──────────────── */
/*
 * Allocates a 32 MB buffer (8192 pages).  Alternates between:
 *   sequential scan  — one TLB miss per new page, then hits; TLB-friendly.
 *   random scan      — stride=4099 pages (prime, coprime with 8192) visits
 *                      every page out-of-order, blowing past the ~1024-entry
 *                      L2 dTLB; almost every access triggers a page-table walk.
 * Prints the ms difference each round so the slowdown is visible live.
 */
static void tlb_worker(const char *color) {
    set_proc_name(color);

    const size_t BUF_MB   = 32;
    const size_t BUF_SIZE = BUF_MB * 1024UL * 1024UL;
    const size_t PG       = 4096;
    const size_t N_PAGES  = BUF_SIZE / PG;   /* 8192 pages */
    const size_t STRIDE   = 4099;             /* prime, coprime with 8192 */

    char *buf = mmap(NULL, BUF_SIZE, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (buf == MAP_FAILED) {
        say(color, "mmap failed: %s", strerror(errno));
        return;
    }
    memset(buf, 1, BUF_SIZE);   /* fault-in all pages so timing is clean */

    say(color, A_BOLD "TLB SABOTAGE" A_RESET
               " – %zu pages (%zu MB), stride=%zu; L2 dTLB holds ~1024 entries",
               N_PAGES, BUF_MB, STRIDE);
    say(color, A_DIM "measure: perf stat -e dTLB-misses,dTLB-loads -p %d sleep 30" A_RESET,
               getpid());

    unsigned long round = 0;
    while (keep_running) {
        struct timespec t0, t1;
        volatile char sink = 0;

        /* sequential — TLB-friendly */
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (size_t i = 0; i < N_PAGES; i++)
            sink ^= buf[i * PG];
        clock_gettime(CLOCK_MONOTONIC, &t1);
        long seq_ms = ((t1.tv_sec  - t0.tv_sec)  * 1000000000L +
                       (t1.tv_nsec - t0.tv_nsec)) / 1000000;

        /* random — TLB-thrashing */
        clock_gettime(CLOCK_MONOTONIC, &t0);
        size_t idx = 0;
        for (size_t i = 0; i < N_PAGES; i++) {
            sink ^= buf[idx * PG];
            idx = (idx + STRIDE) % N_PAGES;
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        long rnd_ms = ((t1.tv_sec  - t0.tv_sec)  * 1000000000L +
                       (t1.tv_nsec - t0.tv_nsec)) / 1000000;

        (void)sink;
        say(color, "round %lu | seq %ld ms | random %ld ms | "
                   A_BOLD "%.1fx slower" A_RESET
                   " – random blows TLB capacity, each page = TLB miss + page-table walk",
                   ++round, seq_ms, rnd_ms,
                   seq_ms > 0 ? (double)rnd_ms / seq_ms : 0.0);
        sleep(2);
    }
    munmap(buf, BUF_SIZE);
}

/* ── worker: combo (CPU + memory + zombies) ─────────────────────────────── */
static void combo_worker(const char *color, int step_mb, int limit_mb, int zlimit) {
    set_proc_name(color);
    say(color, A_BOLD "COMBO IMPOSTOR" A_RESET " – CPU burn + memory leak + zombie bodies");
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

        if (loop % 15 == 0 && used_mb + step_mb <= limit_mb && count < MAX_ALLOCS) {
            size_t bytes = (size_t)step_mb * 1024UL * 1024UL;
            char *p = malloc(bytes);
            if (p) {
                for (size_t i = 0; i < bytes; i += 4096) p[i] = (char)(count + 1);
                blocks[count++] = p;
                used_mb += step_mb;
                say(color, "combo evidence: CPU high + leaked %d MB total", used_mb);
            }
        }

        if (loop % 30 == 0 && zombies < zlimit) {
            pid_t pid = fork();
            if (pid == 0) {
                prctl(PR_SET_NAME, "among_body", 0, 0, 0);
                _exit(99);
            } else if (pid > 0) {
                zombies++;
                say(color, "combo evidence: zombie body PID=%d", pid);
            }
        }
    }
    for (int i = 0; i < count; i++) free(blocks[i]);
}

/* ── role assignment ─────────────────────────────────────────────────────── */
static role_t role_for_index(int idx, const char *sab) {
    /* cyan (index 5) is always the impostor */
    if (idx == 5) {
        if (!strcmp(sab, "cpu"))    return ROLE_CPU;
        if (!strcmp(sab, "memory")) return ROLE_MEMORY;
        if (!strcmp(sab, "zombie")) return ROLE_ZOMBIE;
        if (!strcmp(sab, "orphan")) return ROLE_ORPHAN;
        if (!strcmp(sab, "tlb"))    return ROLE_TLB;
        return ROLE_COMBO;
    }
    switch (idx) {
        case 0: return ROLE_SLEEPER;
        case 1: return ROLE_NORMAL;
        case 2: return ROLE_FD;
        case 3: return ROLE_SLEEPER;
        case 4: return ROLE_NORMAL;
        default: return ROLE_NORMAL;
    }
}

static const char *role_name(role_t r) {
    switch (r) {
        case ROLE_SLEEPER: return "normal sleeper";
        case ROLE_NORMAL:  return "normal heartbeat";
        case ROLE_CPU:     return "CPU sabotage";
        case ROLE_MEMORY:  return "memory sabotage";
        case ROLE_FD:      return "fd baseline";
        case ROLE_ZOMBIE:  return "zombie sabotage";
        case ROLE_ORPHAN:  return "orphan sabotage";
        case ROLE_TLB:     return "TLB sabotage";
        case ROLE_COMBO:   return "COMBO impostor";
        default:           return "unknown";
    }
}

/* ── child entry point ───────────────────────────────────────────────────── */
static void child_main(const char *color, role_t role, const config_t *cfg) {
    signal(SIGTERM, on_signal);
    signal(SIGINT,  on_signal);
    switch (role) {
        case ROLE_SLEEPER: sleeper_worker(color); break;
        case ROLE_NORMAL:  normal_worker(color);  break;
        case ROLE_FD:      fd_worker(color);      break;
        case ROLE_CPU:     cpu_worker(color);     break;
        case ROLE_MEMORY:  memory_worker(color, cfg->mem_step_mb, cfg->mem_limit_mb); break;
        case ROLE_ZOMBIE:  zombie_worker(color, cfg->zombie_limit); break;
        case ROLE_ORPHAN:  orphan_worker(color);  break;
        case ROLE_TLB:     tlb_worker(color);     break;
        case ROLE_COMBO:   combo_worker(color, cfg->mem_step_mb, cfg->mem_limit_mb, cfg->zombie_limit); break;
    }
    say(color, "exiting cleanly");
    _exit(0);
}

/* ── emergency meeting screen (called from main loop, safe) ──────────────── */
static void print_emergency_meeting(bool reveal) {
    const char *sep = "══════════════════════════════════════════════════════════════";
    printf("\n%s╔%s╗%s\n", A_RED_BG, sep, A_RESET);
    printf("%s║%*s%-52s%*s║%s\n", A_RED_BG, 4, "", "⚠   EMERGENCY MEETING   ⚠", 4, "", A_RESET);
    printf("%s╠%s╣%s\n", A_RED_BG, sep, A_RESET);

    printf("%s║  Controller PID: %-6d%35s║%s\n", A_RED_BG, ctrl_pid, "", A_RESET);
    printf("%s╠%s╣%s\n", A_RED_BG, sep, A_RESET);

    for (int i = 0; i < child_count; i++) {
        bool is_imp = (i == 5);
        const char *flag = (is_imp && reveal) ? "  <-- THE IMPOSTOR" : "";
        printf("%s║  %-8s PID=%-7d %-28s%-4s║%s\n",
               A_RED_BG,
               crew_colors[i], children[i],
               g_role_names[i],
               flag, A_RESET);
    }

    printf("%s╠%s╣%s\n", A_RED_BG, sep, A_RESET);
    printf("%s║  INVESTIGATION COMMANDS (open second terminal)%15s║%s\n", A_RED_BG, "", A_RESET);
    printf("%s║  S1: pstree -p %-5d%36s║%s\n", A_RED_BG, ctrl_pid, "", A_RESET);
    printf("%s║  S1: ps -eo pid,ppid,stat,ni,pcpu,pmem,comm | grep among_%5s║%s\n", A_RED_BG, "", A_RESET);
    printf("%s║  S2: pidstat -p ALL 1%40s║%s\n", A_RED_BG, "", A_RESET);
    printf("%s║  S3: vmstat 1%48s║%s\n", A_RED_BG, "", A_RESET);
    printf("%s╚%s╝%s\n\n", A_RED_BG, sep, A_RESET);
    fflush(stdout);

    if (reveal) {
        printf(A_BOLD "REVEAL: cyan (PID %d) was The Impostor – sabotage: %s\n" A_RESET,
               children[5], g_cfg.sabotage);
        fflush(stdout);
    }
}

/* ── print pre-filled hint commands at startup ───────────────────────────── */
static void print_hint_commands(void) {
    /* Build a comma-separated PID list for top */
    char pids[256] = "";
    for (int i = 0; i < child_count; i++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%s%d", i ? "," : "", children[i]);
        strncat(pids, buf, sizeof(pids) - strlen(pids) - 1);
    }

    printf("\n" A_BOLD "── COPY-PASTE INVESTIGATION COMMANDS ──────────────────────" A_RESET "\n");
    printf(A_DIM "# Speaker 1 – Process Detective\n" A_RESET);
    printf("  pstree -p %d\n", ctrl_pid);
    printf("  ps -eo pid,ppid,stat,ni,pcpu,pmem,comm | grep -E 'among_|PID'\n");
    printf("  cat /proc/%d/status          # check the CYAN suspect\n", children[5]);
    printf("  ls -l /proc/%d/fd            # file descriptors\n\n", children[2]);

    printf(A_DIM "# Speaker 2 – Scheduler Detective\n" A_RESET);
    printf("  top -p %s\n", pids);
    printf("  pidstat -p ALL 1\n");
    printf("  renice +10 -p %d            # penalise CYAN live\n\n", children[5]);

    printf(A_DIM "# Speaker 3 – Memory Detective\n" A_RESET);
    printf("  watch -n1 'grep -E VmRSS\\|VmData\\|VmSize /proc/%d/status'\n", children[5]);
    printf("  cat /proc/%d/maps | grep heap\n", children[5]);
    printf("  vmstat 1\n");
    printf("  strace -e brk,mmap,munmap -p %d\n\n", children[5]);

    printf(A_DIM "# Emergency meeting  (from another terminal)\n" A_RESET);
    printf("  kill -USR1 %d               # print meeting screen\n", ctrl_pid);
    printf("  kill -USR2 %d               # same + reveal impostor\n\n", ctrl_pid);

    printf(A_DIM "# Cleanup\n" A_RESET);
    printf("  bash scripts/cleanup.sh\n");
    printf(A_BOLD "────────────────────────────────────────────────────────────\n" A_RESET "\n");
    fflush(stdout);
}

/* ── cleanup helpers ─────────────────────────────────────────────────────── */
static void cleanup_children(void) {
    printf("\n" A_BOLD "[SPACESHIP] Cleaning up child processes…" A_RESET "\n");
    for (int i = 0; i < child_count; i++)
        if (children[i] > 0) kill(children[i], SIGTERM);
    sleep(1);
    for (int i = 0; i < child_count; i++)
        if (children[i] > 0) kill(children[i], SIGKILL);
    while (waitpid(-1, NULL, WNOHANG) > 0) {}
}

/* ── argument parsing ────────────────────────────────────────────────────── */
static void print_usage(const char *prog) {
    printf("Usage: %s [options]\n"
           "  --sabotage cpu|memory|zombie|orphan|tlb|combo  impostor behaviour (default: combo)\n"
           "  --duration SECONDS                         auto-stop after N seconds\n"
           "  --mem-step-mb N                            MB per memory step (default: %d)\n"
           "  --mem-limit-mb N                           memory cap in MB (default: %d)\n"
           "  --zombie-limit N                           max zombie bodies (default: %d)\n"
           "  --reveal                                   print impostor role at startup\n"
           "  --no-color                                 disable ANSI colours\n"
           "  --help\n",
           prog, DEFAULT_MEM_STEP_MB, DEFAULT_MEM_LIMIT_MB, DEFAULT_ZOMBIE_LIMIT);
}

static config_t parse_args(int argc, char **argv) {
    config_t cfg = {"combo", 0, DEFAULT_MEM_STEP_MB, DEFAULT_MEM_LIMIT_MB,
                    DEFAULT_ZOMBIE_LIMIT, false, false};
    for (int i = 1; i < argc; i++) {
        if      (!strcmp(argv[i], "--sabotage")    && i+1<argc) cfg.sabotage    = argv[++i];
        else if (!strcmp(argv[i], "--duration")    && i+1<argc) cfg.duration    = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--mem-step-mb") && i+1<argc) cfg.mem_step_mb = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--mem-limit-mb")&& i+1<argc) cfg.mem_limit_mb= atoi(argv[++i]);
        else if (!strcmp(argv[i], "--zombie-limit")&& i+1<argc) cfg.zombie_limit= atoi(argv[++i]);
        else if (!strcmp(argv[i], "--reveal"))   cfg.reveal   = true;
        else if (!strcmp(argv[i], "--no-color")) cfg.no_color = true;
        else if (!strcmp(argv[i], "--help"))  { print_usage(argv[0]); exit(0); }
        else { fprintf(stderr, "Unknown option: %s\n", argv[i]); print_usage(argv[0]); exit(2); }
    }
    if (cfg.mem_step_mb  <= 0) cfg.mem_step_mb  = DEFAULT_MEM_STEP_MB;
    if (cfg.mem_limit_mb <  cfg.mem_step_mb) cfg.mem_limit_mb = cfg.mem_step_mb;
    if (cfg.zombie_limit <  0) cfg.zombie_limit = 0;
    return cfg;
}

/* ── main ────────────────────────────────────────────────────────────────── */
int main(int argc, char **argv) {
    g_cfg    = parse_args(argc, argv);
    ctrl_pid = getpid();

    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGUSR1, on_sigusr1);   /* kill -USR1 <pid>  → emergency meeting  */
    signal(SIGUSR2, on_sigusr2);   /* kill -USR2 <pid>  → meeting + reveal   */
    prctl(PR_SET_NAME, "amongos_ctrl", 0, 0, 0);

    /* banner */
    printf("\n" A_BOLD
           "╔══════════════════════════════════════════════════════╗\n"
           "║          AMONG OS — The Impostor Process             ║\n"
           "║          CM-204 Operating Systems  Project 2         ║\n"
           "╚══════════════════════════════════════════════════════╝\n"
           A_RESET);
    printf("Controller  PID  : %d\n", ctrl_pid);
    printf("Sabotage mode    : %s\n", g_cfg.sabotage);
    printf("Safety caps      : mem step=%d MB, mem limit=%d MB, zombies=%d\n\n",
           g_cfg.mem_step_mb, g_cfg.mem_limit_mb, g_cfg.zombie_limit);

    /* write PID file for scripts */
    FILE *pf = fopen(".amongos_controller.pid", "w");
    if (pf) { fprintf(pf, "%d\n", ctrl_pid); fclose(pf); }

    FILE *sf = fopen(".amongos_state.txt", "w");
    if (sf) fprintf(sf, "controller %d\n# color pid role\n", ctrl_pid);

    /* fork crew */
    for (int i = 0; i < MAX_CREW; i++) {
        role_t      role  = role_for_index(i, g_cfg.sabotage);
        const char *color = crew_colors[i];
        pid_t       pid   = fork();

        if (pid == 0) {
            if (sf) fclose(sf);
            child_main(color, role, &g_cfg);
        } else if (pid > 0) {
            children[child_count] = pid;
            snprintf(g_role_names[child_count], sizeof(g_role_names[0]),
                     "%s", role_name(role));
            child_count++;

            const char *ac = ansi_for(color);
            bool is_imp = (i == 5);
            printf("  %s%-6s%s PID=%-6d  %s%s%s\n",
                   ac, color, A_RESET, pid,
                   is_imp ? A_BOLD : "", role_name(role), A_RESET);

            if (sf) fprintf(sf, "%s %d %s\n", color, pid, role_name(role));
        } else {
            fprintf(stderr, "fork failed for %s: %s\n", color, strerror(errno));
        }
    }
    if (sf) fclose(sf);

    if (g_cfg.reveal)
        printf("\n" A_BOLD "REVEAL: cyan (PID %d) is the impostor – %s\n" A_RESET,
               children[5], g_cfg.sabotage);

    print_hint_commands();

    /* main loop */
    time_t start = time(NULL);
    while (keep_running) {
        if (g_cfg.duration > 0 && time(NULL) - start >= g_cfg.duration) break;

        if (do_meeting) {
            do_meeting = 0;
            print_emergency_meeting(false);
        }
        if (do_reveal) {
            do_reveal = 0;
            print_emergency_meeting(true);
        }

        sleep(1);
        while (waitpid(-1, NULL, WNOHANG) > 0) {}
    }

    cleanup_children();
    printf(A_BOLD "[SPACESHIP] Demo ended. Run: bash scripts/cleanup.sh\n" A_RESET);
    return 0;
}
