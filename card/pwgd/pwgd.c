/* pwgd.c: the card's terrain-shape daemon for the live server. See pwgd.md.
 *   pwgd [-p PORT] [-w WORKERS] [-s SHM_OFFSET_MIB] [-c DMA_CHANNEL]
 * Serves the shared host-memory region (docs/shm-protocol.md) through DMA channel -c, and TCP on 127.0.0.1:PORT
 * (reached through an ssh -L forward) as the fallback. Defaults: port 25600, 120 workers, offset 1024 MiB,
 * channel 6; -s 0 turns the shared region off. */
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "capfile.h"
#include "cdma.h"
#include "genlayer.h"
#include "shape.h"
#include "vshape.h"

enum { REQ_BYTES = 212, QCAP = 1 << 16, HDR_BYTES = 16, VIA_TCP = 0, VIA_SHM = 1 };
enum { PROTOCOL = 2 };
/* request flags: 1 urgent, 2 the card computes the 10 x 10 biomes (the request's are zero) */
enum { RQ_URGENT = 1, RQ_CARD_BIOMES = 2 };
/* answer flags, the payload's first u32: 1 the 16 x 16 biome slots follow (u16[256]), before the column runs */
enum { AN_BI16 = 1 };

/* One world state: the shape generators, and the GenLayer chains when the capture has them. */
typedef struct {
    world w;
    gl_world g;
} wstate;

/* docs/shm-protocol.md */
enum {
    S_MAGIC = 0, S_VERSION = 4, S_SESSION = 8, S_WLEN = 16, S_POSTED = 64, S_COMPLETED = 128, S_CSESSION = 192,
    S_CWORKERS = 200, S_CONSUMED = 256, S_ALIVE = 320
};
#define S_WORLD (64u << 10)
#define S_WORLD_MAX ((2u << 20) - S_WORLD)
#define S_REQ (2u << 20)
#define S_REQ_N 4096u
#define S_REQ_SIZE 256u
#define S_COMPL (3u << 20)
#define S_SLOTS (4u << 20)
#define S_SLOT_N 2048u
#define S_SLOT_SIZE 135168u
#define STAGE_SIZE S_SLOT_SIZE
#define MAX_LINES_BYTES (16383u * 64u)

typedef struct {
    int cx, cz, via;
    unsigned gen;
    uint32_t flags;
    const wstate *ws;
    uint16_t bi10[100];
} req;

static req queue[QCAP];
static int qhead, qcount;
static pthread_mutex_t qm = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t qc = PTHREAD_COND_INITIALIZER;

static pthread_mutex_t wm = PTHREAD_MUTEX_INITIALIZER; /* the TCP connection and its writes */
static int conn = -1;
static volatile unsigned tcp_gen, shm_gen;
static long served_tcp, served_shm, computed_us;
static int nworkers = 120;

typedef struct {
    uint8_t *virt;
    uint64_t phys;
} stage_t;
static stage_t *stage;

/* the shared region */
static cdma D;
static volatile uint8_t *shm; /* its header page, uncached through /dev/phihost */
static uint64_t part_card;    /* the card address of our part of the region */
static pthread_mutex_t am = PTHREAD_MUTEX_INITIALIZER;
static uint64_t answer_next;

static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

static void nap_us(long us)
{
    struct timespec t = { 0, us * 1000 };
    nanosleep(&t, NULL);
}

static void say(const char *fmt, ...)
{
    time_t t = time(NULL);
    char ts[32];
    strftime(ts, sizeof ts, "%F %T", gmtime(&t));
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "%s pwgd: ", ts);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

/* x87 precision control to 53 bits, round to nearest: Java's double (see lib/shape.md). */
static void fpcw_set(void)
{
    unsigned short cw = 0x027F;
    __asm__ volatile("fldcw %0" : : "m"(cw));
}

static inline void fence(void) { __asm__ volatile("lock addq $0, (%%rsp)" ::: "memory"); }
static inline uint64_t hr64(int off) { return *(volatile uint64_t *)(shm + off); }
static inline uint32_t hr32(int off) { return *(volatile uint32_t *)(shm + off); }
static inline void hw64(int off, uint64_t v) { *(volatile uint64_t *)(shm + off) = v; }
static inline void hw32(int off, uint32_t v) { *(volatile uint32_t *)(shm + off) = v; }

static int read_full(int fd, void *buf, size_t n)
{
    uint8_t *p = buf;
    while (n) {
        ssize_t r = read(fd, p, n);
        if (r <= 0) {
            if (r < 0 && errno == EINTR) continue;
            return -1;
        }
        p += r;
        n -= (size_t)r;
    }
    return 0;
}

static int write_full(int fd, const void *buf, size_t n)
{
    const uint8_t *p = buf;
    while (n) {
        ssize_t r = write(fd, p, n);
        if (r <= 0) {
            if (r < 0 && errno == EINTR) continue;
            return -1;
        }
        p += r;
        n -= (size_t)r;
    }
    return 0;
}

/* Urgent requests (the chunk the server is waiting on) go to the front. */
static void push(const req *r, int urgent)
{
    pthread_mutex_lock(&qm);
    if (qcount < QCAP) {
        if (urgent) {
            qhead = (qhead + QCAP - 1) % QCAP;
            queue[qhead] = *r;
        } else {
            queue[(qhead + qcount) % QCAP] = *r;
        }
        qcount++;
        pthread_cond_signal(&qc);
    }
    pthread_mutex_unlock(&qm);
}

/* One answer into the shared region: slot a % 2048 by DMA, then n + 1 into completion entry a % 2048. */
static void shm_deliver(stage_t *st, const req *r, uint32_t len, uint32_t us)
{
    pthread_mutex_lock(&am);
    uint64_t a = answer_next++;
    pthread_mutex_unlock(&am);
    while (a - hr64(S_COMPLETED) >= S_SLOT_N) {
        if (r->gen != shm_gen) return;
        nap_us(20);
    }
    uint8_t *h = st->virt;
    memset(h, 0, 64);
    memcpy(h, &r->cx, 4);
    memcpy(h + 4, &r->cz, 4);
    memcpy(h + 8, &len, 4);
    memcpy(h + 12, &us, 4);
    uint64_t tag = a + 1;
    memcpy(h + 16, &tag, 8);
    uint32_t bytes = (64 + len + 63) / 64 * 64;
    uint64_t seq = cdma_submit(&D, st->phys, part_card + S_SLOTS + (a % S_SLOT_N) * S_SLOT_SIZE, bytes,
                               part_card + S_COMPL + (a % S_SLOT_N) * 64, tag);
    if (!seq || cdma_wait(&D, seq, 2.0)) {
        say("DMA answer %llu for chunk %d,%d did not complete", (unsigned long long)a, r->cx, r->cz);
        return;
    }
    __atomic_add_fetch(&served_shm, 1, __ATOMIC_RELAXED);
}

static void *worker(void *arg)
{
    stage_t *st = &stage[(long)arg];
    fpcw_set();
    static __thread uint16_t blocks[65536];
    gl_arena arena;
    if (gl_arena_init(&arena, 1 << 18)) {
        say("worker %ld: no memory for the layer arena", (long)arg);
        return NULL;
    }
    for (;;) {
        pthread_mutex_lock(&qm);
        while (qcount == 0) pthread_cond_wait(&qc, &qm);
        req r = queue[qhead];
        qhead = (qhead + 1) % QCAP;
        qcount--;
        pthread_mutex_unlock(&qm);
        if (r.gen != (r.via == VIA_SHM ? shm_gen : tcp_gen)) continue;

        double t0 = now();
        const world *w = &r.ws->w;
        const gl_world *g = &r.ws->g;
        /* the payload after the 64-byte header: flags, the 16 x 16 biome slots when the layers are here, the runs */
        uint8_t *pay = st->virt + 64;
        uint32_t aflags = 0, off = 4;
        if (g->have) {
            if (r.flags & RQ_CARD_BIOMES) {
                gl_slots10(g, &arena, r.cx, r.cz, r.bi10);
                for (int i = 0; i < 100; i++)
                    if (r.bi10[i] == 0xFFFF) r.bi10[i] = 0; /* a null biome: Java would throw; never seen */
            }
            uint16_t b16[256];
            gl_slots16(g, &arena, r.cx, r.cz, b16);
            memcpy(pay + off, b16, sizeof b16);
            off += sizeof b16;
            aflags |= AN_BI16;
        }
        memcpy(pay, &aflags, 4);
        {
            /* the 3D octaves on the vector unit (lib/vshape.c), the rest scalar */
            double o0[25], o1[825], o2[825], o3[825], field[825];
            noise_octaves(&w->gen[0], o0, r.cx * 4, 10, r.cz * 4, 5, 1, 5, 200.0, 1.0, 200.0);
            vshape_octaves3d(w, r.cx, r.cz, o1, o2, o3);
            shape_combine(w, r.bi10, o0, o1, o2, o3, field);
            shape_interp(field, blocks);
        }
        uint32_t len = off + (uint32_t)shape_rle(blocks, pay + off);
        uint32_t us = (uint32_t)((now() - t0) * 1e6);
        __atomic_add_fetch(&computed_us, us, __ATOMIC_RELAXED);

        if (r.via == VIA_SHM) {
            shm_deliver(st, &r, len, us);
            continue;
        }
        uint8_t *h = st->virt + 64 - HDR_BYTES; /* the TCP header just before the runs */
        memcpy(h, &r.cx, 4);
        memcpy(h + 4, &r.cz, 4);
        memcpy(h + 8, &len, 4);
        memcpy(h + 12, &us, 4);
        pthread_mutex_lock(&wm);
        if (r.gen == tcp_gen && conn >= 0 && write_full(conn, h, HDR_BYTES + len) == 0) served_tcp++;
        pthread_mutex_unlock(&wm);
    }
    return NULL;
}

/* A world state from a blob (taken over), or NULL. */
static wstate *load_world(uint8_t *blob, size_t len)
{
    capfile f;
    wstate *w = calloc(1, sizeof *w);
    if (!w || cap_wrap(&f, blob, len, "PWGW") || world_load(&w->w, &f, "world state") ||
        gl_load(&w->g, &f, "world state"))
        return NULL;
    return w; /* never freed: a worker may still hold the pointer from a request of an earlier session */
}

/* The shared-region dispatcher: sessions, requests in by DMA, the heartbeat. */
static void *shm_main(void *arg)
{
    uint64_t off = (uint64_t)(uintptr_t)arg << 20;
    FILE *sw = fopen("/proc/swaps", "r");
    char line[256];
    while (sw && fgets(line, sizeof line, sw))
        if (strncmp(line, "/dev/phiblk1", 12) == 0) {
            say("shared region refused: the card swaps onto the whole host-memory region (/dev/phiblk1); TCP only");
            fclose(sw);
            return NULL;
        }
    if (sw) fclose(sw);
    int fd = open("/dev/phihost", O_RDWR | O_SYNC);
    shm = fd < 0 ? MAP_FAILED : mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)off);
    if (shm == MAP_FAILED) { say("shared region refused: /dev/phihost: %s; TCP only", strerror(errno)); return NULL; }
    if (cdma_open(&D, 6)) { say("shared region refused: no DMA channel; TCP only"); return NULL; }
    if (off + S_SLOTS + (uint64_t)S_SLOT_N * S_SLOT_SIZE > D.region_size) {
        say("shared region refused: offset %llu MiB leaves too little of the %llu MiB region",
            (unsigned long long)(off >> 20), (unsigned long long)(D.region_size >> 20));
        return NULL;
    }
    part_card = D.region_addr + off;
    uint64_t land_phys[1];
    uint8_t *land = huge_alloc(2u << 20, land_phys, 1);
    if (!land) { say("shared region refused: no huge page for the landing area; TCP only"); return NULL; }
    say("shared region: part at %llu MiB (card address 0x%llx), DMA channel %d", (unsigned long long)(off >> 20),
        (unsigned long long)part_card, D.chan);

    uint64_t session = 0, consumed = 0, beat = 0;
    wstate *w = NULL;
    double last_beat = 0;
    for (;;) {
        double t = now();
        if (t - last_beat > 0.1) {
            hw64(S_ALIVE, ++beat);
            last_beat = t;
        }
        if (memcmp((const void *)shm, "PWGS", 4) != 0 || hr32(S_VERSION) != PROTOCOL) {
            nap_us(1000);
            continue;
        }
        uint64_t s = hr64(S_SESSION);
        if (s && s != session) {
            uint32_t len = hr32(S_WLEN);
            uint32_t bytes = (len + 63) / 64 * 64;
            if (!len || bytes > S_WORLD_MAX || bytes > (2u << 20)) {
                say("session %llu: world state of %u bytes refused", (unsigned long long)s, len);
                session = s;
                continue;
            }
            int ok = 1;
            for (uint32_t done = 0; done < bytes && ok;) {
                uint32_t n = bytes - done < MAX_LINES_BYTES ? bytes - done : MAX_LINES_BYTES;
                uint64_t seq = cdma_submit(&D, part_card + S_WORLD + done, land_phys[0] + done, n, 0, 0);
                ok = seq && cdma_wait(&D, seq, 2.0) == 0;
                done += n;
            }
            uint8_t *blob = malloc(len);
            if (ok && blob) memcpy(blob, land, len);
            wstate *nw = ok && blob ? load_world(blob, len) : NULL;
            if (!nw) {
                say("session %llu: world state did not load", (unsigned long long)s);
                session = s;
                continue;
            }
            pthread_mutex_lock(&am);
            answer_next = 0;
            pthread_mutex_unlock(&am);
            w = nw;
            shm_gen++;
            consumed = 0;
            served_shm = 0;
            hw64(S_CONSUMED, 0);
            hw32(S_CWORKERS, (uint32_t)nworkers);
            fence();
            hw64(S_CSESSION, s);
            session = s;
            say("session %llu: world state %u bytes, octaves %d %d %d %d, amplified %d, biome layers %s",
                (unsigned long long)s, len, w->w.gen[0].n, w->w.gen[1].n, w->w.gen[2].n, w->w.gen[3].n, w->w.amplified,
                w->g.have ? "on the card" : "from the server");
        }
        uint64_t posted = session ? hr64(S_POSTED) : 0;
        if (!session || posted <= consumed || posted - consumed > S_REQ_N) {
            nap_us(50);
            continue;
        }
        uint32_t first = (uint32_t)(consumed % S_REQ_N), n = (uint32_t)(posted - consumed);
        if (n > S_REQ_N - first) n = S_REQ_N - first;
        uint64_t seq = cdma_submit(&D, part_card + S_REQ + first * S_REQ_SIZE, land_phys[0], n * S_REQ_SIZE, 0, 0);
        if (!seq || cdma_wait(&D, seq, 2.0)) {
            say("DMA pull of %u requests did not complete", n);
            nap_us(1000);
            continue;
        }
        for (uint32_t i = 0; i < n; i++) {
            const uint8_t *e = land + i * S_REQ_SIZE;
            req r = { .cx = rd32(e), .cz = rd32(e + 4), .via = VIA_SHM, .gen = shm_gen, .flags = (uint32_t)rd32(e + 8), .ws = w };
            for (int k = 0; k < 100; k++) r.bi10[k] = rd16(e + 12 + 2 * k);
            push(&r, r.flags & RQ_URGENT);
        }
        consumed += n;
        hw64(S_CONSUMED, consumed);
    }
    return NULL;
}

/* One TCP connection: the world state, then requests until it closes. */
static void serve(int fd)
{
    uint8_t hello[12];
    if (read_full(fd, hello, 12) || memcmp(hello, "PWGH", 4) != 0 || rd32(hello + 4) != PROTOCOL) {
        say("tcp: bad hello, closing");
        return;
    }
    uint32_t len = (uint32_t)rd32(hello + 8);
    uint8_t *blob = malloc(len);
    wstate *w;
    if (!blob || read_full(fd, blob, len) || !(w = load_world(blob, len))) {
        say("tcp: bad world state (%u bytes), closing", len);
        return;
    }
    pthread_mutex_lock(&wm);
    conn = fd;
    tcp_gen++;
    unsigned g = tcp_gen;
    served_tcp = 0;
    uint8_t ready[12];
    memcpy(ready, "PWGR", 4);
    uint32_t one = PROTOCOL, workers = (uint32_t)nworkers;
    memcpy(ready + 4, &one, 4);
    memcpy(ready + 8, &workers, 4);
    write_full(fd, ready, 12);
    pthread_mutex_unlock(&wm);
    say("tcp: connected: world state %u bytes, octaves %d %d %d %d, amplified %d, biome layers %s", len,
        w->w.gen[0].n, w->w.gen[1].n, w->w.gen[2].n, w->w.gen[3].n, w->w.amplified, w->g.have ? "on the card" : "from the server");

    long requests = 0;
    uint8_t buf[REQ_BYTES];
    double t0 = now();
    while (read_full(fd, buf, REQ_BYTES) == 0) {
        req r = { .cx = rd32(buf), .cz = rd32(buf + 4), .via = VIA_TCP, .gen = g, .flags = (uint32_t)rd32(buf + 8), .ws = w };
        for (int i = 0; i < 100; i++) r.bi10[i] = rd16(buf + 12 + 2 * i);
        push(&r, r.flags & RQ_URGENT);
        requests++;
    }
    pthread_mutex_lock(&wm);
    conn = -1;
    tcp_gen++;
    say("tcp: disconnected after %.1f s: %ld requests, %ld served", now() - t0, requests, served_tcp);
    pthread_mutex_unlock(&wm);
}

int main(int argc, char **argv)
{
    int port = 25600, shm_mib = 1024;
    for (int i = 1; i + 1 < argc; i += 2) {
        if (!strcmp(argv[i], "-p")) port = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "-w")) nworkers = atoi(argv[i + 1]);
        else if (!strcmp(argv[i], "-s")) shm_mib = atoi(argv[i + 1]);
        else { fprintf(stderr, "usage: pwgd [-p PORT] [-w WORKERS] [-s SHM_OFFSET_MIB]\n"); return 2; }
    }
    signal(SIGPIPE, SIG_IGN);
    fpcw_set();

    /* every worker's answer staging: 15 slices of 132 KiB per 2 MiB huge page, physically contiguous for DMA */
    int per = (2u << 20) / STAGE_SIZE, pages = (nworkers + per - 1) / per;
    uint64_t *phys = calloc((size_t)pages, sizeof *phys);
    uint8_t *mem = huge_alloc((size_t)pages << 21, phys, pages);
    stage = calloc((size_t)nworkers, sizeof *stage);
    if (!mem || !stage) { say("no %d huge pages for the workers' staging", pages); return 1; }
    for (int i = 0; i < nworkers; i++) {
        stage[i].virt = mem + ((size_t)(i / per) << 21) + (size_t)(i % per) * STAGE_SIZE;
        stage[i].phys = phys[i / per] + (uint64_t)(i % per) * STAGE_SIZE;
    }

    int ls = socket(AF_INET, SOCK_STREAM, 0), on = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
    struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons((uint16_t)port) };
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(ls, (struct sockaddr *)&a, sizeof a) || listen(ls, 4)) {
        say("cannot listen on 127.0.0.1:%d: %s", port, strerror(errno));
        return 1;
    }
    for (long i = 0; i < nworkers; i++) {
        pthread_t t;
        pthread_create(&t, NULL, worker, (void *)i);
    }
    say("listening on 127.0.0.1:%d with %d workers; staging in %d huge pages", port, nworkers, pages);
    if (shm_mib > 0) {
        pthread_t t;
        pthread_create(&t, NULL, shm_main, (void *)(uintptr_t)shm_mib);
    }
    for (;;) {
        int fd = accept(ls, NULL, NULL);
        if (fd < 0) continue;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof on);
        serve(fd);
        close(fd);
    }
}
