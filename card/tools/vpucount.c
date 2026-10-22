/* vpucount.c: counts the card's VPU instructions in user mode with the core's performance counters, for a running
 * process (every thread) or a command. See vpucount.md.
 *   vpucount [-e CODE,...] -p PID -t SECONDS
 *   vpucount [-e CODE,...] -- COMMAND [ARGS]
 * CODE is a KNC event: event in bits 0-7, unit mask in bits 8-15 (the cpu PMU's format, type 4). */
#include <dirent.h>
#include <signal.h>
#include <errno.h>
#include <linux/perf_event.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* libpfm4 lib/events/intel_knc_events.h */
static const struct {
    unsigned code;
    const char *name;
} known[] = {
    { 0x2016, "VPU_INSTRUCTIONS_EXECUTED" }, { 0x2018, "VPU_ELEMENTS_ACTIVE" },
    { 0x2017, "VPU_INSTRUCTIONS_EXECUTED_V_PIPE" }, { 0x2005, "VPU_STALL_REG" },
    { 0x2000, "VPU_DATA_READ" }, { 0x2001, "VPU_DATA_WRITE" },
    { 0x0016, "INSTRUCTIONS_EXECUTED" }, { 0x002a, "CPU_CLK_UNHALTED" },
};

static void stop(int sig) { (void)sig; }

static const char *event_name(unsigned code)
{
    for (size_t i = 0; i < sizeof known / sizeof known[0]; i++)
        if (known[i].code == code) return known[i].name;
    return "raw";
}

static int open_event(unsigned code, pid_t pid, int inherit)
{
    struct perf_event_attr a;
    memset(&a, 0, sizeof a);
    a.size = sizeof a;
    a.type = PERF_TYPE_RAW;
    a.config = code;
    a.exclude_kernel = 1;
    a.exclude_hv = 1;
    a.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;
    a.inherit = inherit;
    a.disabled = inherit;
    a.enable_on_exec = inherit;
    return (int)syscall(__NR_perf_event_open, &a, pid, -1, -1, 0);
}

typedef struct {
    uint64_t value, enabled, running;
} reading;

#define MAXEV 8
#define MAXFD 4096

int main(int argc, char **argv)
{
    /* one event by default: on the 7120A a second event read 0, alone or grouped (2026-10-22) */
    unsigned ev[MAXEV] = { 0x2016 };
    int nev = 1, pid = 0;
    double secs = 0;
    int i = 1;
    for (; i < argc; i++) {
        if (!strcmp(argv[i], "-e") && i + 1 < argc) {
            nev = 0;
            for (char *s = strtok(argv[++i], ","); s && nev < MAXEV; s = strtok(NULL, ",")) ev[nev++] = (unsigned)strtoul(s, NULL, 0);
        } else if (!strcmp(argv[i], "-p") && i + 1 < argc) pid = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-t") && i + 1 < argc) secs = atof(argv[++i]);
        else if (!strcmp(argv[i], "--")) { i++; break; }
        else { fprintf(stderr, "usage: vpucount [-e CODE,...] -p PID -t SECONDS | vpucount [-e CODE,...] -- CMD [ARGS]\n"); return 2; }
    }
    static int fd[MAXFD];
    static int fdev[MAXFD];
    int nfd = 0, threads = 0;
    pid_t child = 0;
    struct timespec t0, t1;

    if (pid) {
        /* every thread of the process: an event per thread, each on its own (a group member read 0 on the card,
         * 2026-10-22: only the leader counted) */
        char path[64];
        snprintf(path, sizeof path, "/proc/%d/task", pid);
        DIR *d = opendir(path);
        if (!d) { perror(path); return 1; }
        struct dirent *e;
        while ((e = readdir(d))) {
            if (e->d_name[0] == '.') continue;
            pid_t tid = atoi(e->d_name);
            for (int k = 0; k < nev && nfd < MAXFD; k++) {
                int f = open_event(ev[k], tid, 0);
                if (f < 0) { fprintf(stderr, "perf_event_open 0x%x on thread %d: %s\n", ev[k], tid, strerror(errno)); return 1; }
                fd[nfd] = f;
                fdev[nfd++] = k;
            }
            threads++;
        }
        closedir(d);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        struct timespec ts = { (time_t)secs, (long)((secs - (double)(time_t)secs) * 1e9) };
        /* SIGTERM or SIGINT ends the window early (the counts are still printed) */
        struct sigaction sa;
        memset(&sa, 0, sizeof sa);
        sa.sa_handler = stop;
        sigaction(SIGTERM, &sa, NULL);
        sigaction(SIGINT, &sa, NULL);
        nanosleep(&ts, NULL);
    } else if (i < argc) {
        int go[2];
        if (pipe(go)) { perror("pipe"); return 1; }
        child = fork();
        if (child == 0) {
            char c;
            close(go[1]);
            if (read(go[0], &c, 1) != 1) _exit(127);
            execvp(argv[i], argv + i);
            perror(argv[i]);
            _exit(127);
        }
        close(go[0]);
        for (int k = 0; k < nev; k++) {
            int f = open_event(ev[k], child, 1);
            if (f < 0) { fprintf(stderr, "perf_event_open 0x%x: %s\n", ev[k], strerror(errno)); return 1; }
            fd[nfd] = f;
            fdev[nfd++] = k;
        }
        threads = 1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        if (write(go[1], "g", 1) != 1) return 1;
        close(go[1]);
        int st;
        waitpid(child, &st, 0);
        if (!WIFEXITED(st) || WEXITSTATUS(st)) fprintf(stderr, "vpucount: the command ended with status %d\n", st);
    } else {
        fprintf(stderr, "vpucount: -p PID -t SECONDS or -- COMMAND\n");
        return 2;
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);

    double total[MAXEV] = { 0 };
    uint64_t en[MAXEV] = { 0 }, run[MAXEV] = { 0 };
    for (int k = 0; k < nfd; k++) {
        reading r;
        if (read(fd[k], &r, sizeof r) != (ssize_t)sizeof r) continue;
        /* scaled by enabled / running, which differ only if the counters were multiplexed */
        total[fdev[k]] += r.running ? (double)r.value * (double)r.enabled / (double)r.running : 0.0;
        en[fdev[k]] += r.enabled;
        run[fdev[k]] += r.running;
        close(fd[k]);
    }
    double s = (double)(t1.tv_sec - t0.tv_sec) + 1e-9 * (double)(t1.tv_nsec - t0.tv_nsec);
    printf("vpucount: %s %d, %d thread(s), %.3f s, user mode\n", pid ? "pid" : "command pid", pid ? pid : child, threads, s);
    for (int k = 0; k < nev; k++)
        printf("  0x%04x %-34s %18.0f  (counted %.1f %% of the time enabled)\n", ev[k], event_name(ev[k]), total[k],
               en[k] ? 100.0 * (double)run[k] / (double)en[k] : 0.0);
    return 0;
}
