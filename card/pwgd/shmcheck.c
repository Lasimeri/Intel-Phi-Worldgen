/* shmcheck.c: drives pwgd through the shared host-memory region with a capture and checks every block. See
 * shmcheck.md.   shmcheck CAPTURE_DIR [ROUNDS [OFFSET_MIB [REGION_FILE]]]
 * Protocol 2: with the layers in world.bin the card computes the biomes (flag 2) and its 16 x 16 biomes are checked
 * against the capture too. */
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "capfile.h"
#include "carve.h"
#include "genlayer.h"
#include "shape.h"

/* docs/shm-protocol.md */
enum {
    S_MAGIC = 0, S_VERSION = 4, S_SESSION = 8, S_WLEN = 16, S_POSTED = 64, S_COMPLETED = 128, S_CSESSION = 192,
    S_CWORKERS = 200, S_CONSUMED = 256, S_ALIVE = 320
};
#define S_WORLD (64u << 10)
#define S_REQ (2u << 20)
#define S_REQ_N 4096u
#define S_COMPL (3u << 20)
#define S_SLOTS (4u << 20)
#define S_SLOT_N 2048u
#define S_SLOT_SIZE 135168u
#define PART (S_SLOTS + S_SLOT_N * (size_t)S_SLOT_SIZE)

static uint8_t *P;
static inline volatile uint64_t *q64(size_t off) { return (volatile uint64_t *)(P + off); }

static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

typedef struct {
    int cx, cz;
    uint16_t bi10[100];
    uint16_t *want;
    uint16_t bi16[256];
    uint16_t *ravn;
    uint8_t *meta, *ssta;
    uint32_t nssta;
} chunk;

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: shmcheck CAPTURE_DIR [ROUNDS [OFFSET_MIB [REGION_FILE]]]\n"); return 2; }
    int rounds = argc > 2 ? atoi(argv[2]) : 1;
    size_t off = (size_t)(argc > 3 ? atoi(argv[3]) : 1024) << 20;
    const char *file = argc > 4 ? argv[4] : "/dev/shm/phi-hostmem";

    char path[4096];
    snprintf(path, sizeof path, "%s/world.bin", argv[1]);
    capfile wf;
    gl_world G;
    if (cap_load(&wf, path, "PWGW") || gl_load(&G, &wf, path)) return 1;
    printf("biomes: %s\n", G.have ? "computed by the card from coordinates" : "sent with the requests");
    surf_world SW;
    static carve_world KW;
    if (surf_load(&SW, &wf, path) || carve_load(&KW, &wf, path)) return 1;
    int whole = G.have && SW.have && KW.have;
    printf("whole chunks: %s\n", whole ? "asked for (surface, caves, ravines on the card)" : "not asked for (the capture lacks the tables)");
    chunk *cs = NULL;
    int n = 0, cap = 0;
    DIR *d = opendir(argv[1]);
    struct dirent *e;
    while (d && (e = readdir(d))) {
        if (strncmp(e->d_name, "c_", 2) != 0) continue;
        snprintf(path, sizeof path, "%s/%s", argv[1], e->d_name);
        capfile cf;
        if (cap_load(&cf, path, "PWGC")) return 1;
        if (n == cap) cs = realloc(cs, sizeof(chunk) * (size_t)(cap = cap ? cap * 2 : 64));
        const uint8_t *pos = cap_need(&cf, path, "CHNK", 8), *bi = cap_need(&cf, path, "BI10", 200);
        const uint8_t *sh = cap_need(&cf, path, "SHAP", 131072);
        const uint8_t *b16 = cap_need(&cf, path, "BI16", 512);
        cs[n].cx = rd32(pos);
        cs[n].cz = rd32(pos + 4);
        for (int i = 0; i < 100; i++) cs[n].bi10[i] = rd16(bi + 2 * i);
        for (int i = 0; i < 256; i++) cs[n].bi16[i] = rd16(b16 + 2 * i);
        uint32_t lr = 0, lm = 0, ls = 0;
        const uint8_t *rv = cap_section(&cf, "RAVN", &lr), *mt = cap_section(&cf, "META", &lm), *ss = cap_section(&cf, "SSTA", &ls);
        cs[n].ravn = NULL;
        if (rv && mt && ss && lr == 131072 && lm == 65536) {
            cs[n].ravn = malloc(131072);
            cs[n].meta = malloc(65536);
            cs[n].ssta = malloc(ls);
            for (int i = 0; i < 65536; i++) cs[n].ravn[i] = rd16(rv + 2 * i);
            memcpy(cs[n].meta, mt, 65536);
            memcpy(cs[n].ssta, ss, ls);
            cs[n].nssta = (uint32_t)rd32(ss);
        }
        cs[n].want = malloc(131072);
        for (int i = 0; i < 65536; i++) cs[n].want[i] = rd16(sh + 2 * i);
        n++;
        cap_free(&cf);
    }
    if (d) closedir(d);

    int fd = open(file, O_RDWR);
    P = fd < 0 ? MAP_FAILED : mmap(NULL, PART, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)off);
    if (P == MAP_FAILED) { perror(file); return 1; }
    uint64_t beat0 = *q64(S_ALIVE);
    usleep(300000);
    printf("card heartbeat: %llu -> %llu in 0.3 s\n", (unsigned long long)beat0, (unsigned long long)*q64(S_ALIVE));

    /* a session: completions cleared, counters zero, the world state, then the session raised */
    double t0 = now();
    memset(P + S_COMPL, 0, S_SLOT_N * 64);
    *q64(S_POSTED) = 0;
    *q64(S_COMPLETED) = 0;
    memcpy(P + S_WORLD, wf.data, wf.size);
    *(volatile uint32_t *)(P + S_WLEN) = (uint32_t)wf.size;
    *(volatile uint32_t *)(P + S_VERSION) = 3;
    memcpy(P + S_MAGIC, "PWGS", 4);
    __sync_synchronize();
    uint64_t session = *q64(S_SESSION) + 1;
    *q64(S_SESSION) = session;
    while (*q64(S_CSESSION) != session)
        if (now() - t0 > 5) { fprintf(stderr, "no session echo from the card in 5 s\n"); return 1; }
    printf("session %llu up in %.3f s: %u card workers\n", (unsigned long long)session, now() - t0,
           *(volatile uint32_t *)(P + S_CWORKERS));

    uint64_t posted = 0, completed = 0;
    long bad = 0, badb = 0, bytes = 0, total = (long)n * rounds, wholes = 0, shapes = 0;
    double first = 0, tb = now();
    for (int r = 0; r < rounds; r++) {
        for (int i = 0; i < n; i++) {
            while (posted - *q64(S_CONSUMED) >= S_REQ_N) {}
            uint8_t *q = P + S_REQ + (posted % S_REQ_N) * 256;
            uint32_t flags = (G.have ? 2 : 0) | (whole ? 4 : 0);
            memcpy(q, &cs[i].cx, 4);
            memcpy(q + 4, &cs[i].cz, 4);
            memcpy(q + 8, &flags, 4);
            if (G.have) memset(q + 12, 0, 200);
            else memcpy(q + 12, cs[i].bi10, 200);
            posted++;
        }
        __sync_synchronize();
        *q64(S_POSTED) = posted;
        /* take this round's answers before posting the next, so no more than 2048 are ever outstanding */
        while (completed < posted) {
            volatile uint64_t *tag = q64(S_COMPL + (completed % S_SLOT_N) * 64);
            double tw = now();
            while (*tag != completed + 1)
                if (now() - tw > 5) { fprintf(stderr, "answer %llu not in after 5 s\n", (unsigned long long)completed); return 1; }
            if (!first) first = now() - tb;
            __sync_synchronize();
            const uint8_t *slot = P + S_SLOTS + (completed % S_SLOT_N) * S_SLOT_SIZE;
            int cx = rd32(slot), cz = rd32(slot + 4);
            uint32_t len = (uint32_t)rd32(slot + 8);
            bytes += len;
            int i;
            for (i = 0; i < n && !(cs[i].cx == cx && cs[i].cz == cz); i++) {}
            /* the payload: u32 flags, the 16 x 16 biome slots (flag 1), then the column runs */
            const uint8_t *pay = slot + 64;
            uint32_t af = (uint32_t)rd32(pay), o = 4;
            if (af & 1) {
                int okb = i < n;
                for (int k = 0; okb && k < 256; k++) {
                    uint16_t s = rd16(pay + 4 + 2 * k);
                    if ((s == 0xFFFF ? 0xFFFF : G.bid[s]) != cs[i].bi16[k]) okb = 0;
                }
                if (!okb && badb++ == 0) printf("first biome difference: chunk %d,%d\n", cx, cz);
                o += 512;
            }
            if (af & 2) { /* the whole chunk: end states, then (id, meta) runs per column */
                wholes++;
                int okw = i < n && cs[i].ravn;
                uint16_t ns = (uint16_t)rd16(pay + o);
                const uint8_t *q = pay + o + 2;
                for (int k = 0; okw && k < ns; k++, q += 8) {
                    int found = 0;
                    for (uint32_t m = 0; m < cs[i].nssta; m++) {
                        const uint8_t *e = cs[i].ssta + 4 + 8 * m;
                        if (rd16(e) != rd16(q)) continue;
                        found = 1;
                        if (rd16(e + 2) != rd16(q + 2) || rd16(e + 4) != rd16(q + 4) || e[6] != q[6]) okw = 0;
                    }
                    if (!found) okw = 0;
                }
                int col = 0, yy = 0;
                while (okw && col < 256 && q + 4 <= pay + len) {
                    uint16_t id = rd16(q);
                    for (int k = 0; k <= q[3]; k++, yy++) {
                        int at = (col << 8) | yy;
                        if (cs[i].ravn[at] != id || cs[i].meta[at] != q[2]) okw = 0;
                    }
                    q += 4;
                    if (yy == 256) { yy = 0; col++; }
                }
                if (col != 256) okw = 0;
                if (!okw && bad++ == 0) printf("first whole-chunk difference: chunk %d,%d\n", cx, cz);
                completed++;
                *q64(S_COMPLETED) = completed;
                continue;
            }
            shapes++;
            int ok = i < n, c = 0, y = 0;
            for (uint32_t p = o; ok && p + 1 < len && c < 256; p += 2) {
                uint16_t id = pay[p] == 1 ? BLOCK_STONE : pay[p] == 2 ? BLOCK_WATER : BLOCK_NULL;
                for (int k = 0; k <= pay[p + 1]; k++, y++)
                    if (cs[i].want[(c << 8) | y] != id) ok = 0;
                if (y == 256) { y = 0; c++; }
            }
            if (c != 256) ok = 0;
            if (!ok && bad++ == 0) printf("first difference: chunk %d,%d\n", cx, cz);
            completed++;
            *q64(S_COMPLETED) = completed;
        }
    }
    double s = now() - tb;
    printf("%ld whole chunks, %ld shapes only\n", wholes, shapes);
    printf("%ld chunks in %.3f s: %.1f chunks/s, first answer after %.3f ms, %.0f bytes a chunk, %ld differ, %ld biome arrays differ\n",
           total, s, total / s, first * 1e3, (double)bytes / (double)total, bad, badb);
    return bad || badb ? 1 : 0;
}
