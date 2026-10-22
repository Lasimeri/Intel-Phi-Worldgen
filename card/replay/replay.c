/* replay.c: replays a capture's shape stage and compares every bit, then times it. See replay.md.
 *   replay DIR [THREADS [ROUNDS [scalar|vector]]]   (vector: the 3D octaves on the vector unit, card build only) */
#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "capfile.h"
#include "genlayer.h"
#include "surface.h"
#include "shape.h"
#ifdef REPLAY_VPU
#include "vshape.h"
#endif

typedef struct {
    int cx, cz;
    uint16_t bi10[100];
} job;

static world W;
static gl_world G;
static surf_world S;
static int surf_skipped, surf_states;
static job *jobs;
static int njobs, nthreads = 1, rounds = 3, use_vector;

static unsigned short fpcw_get(void)
{
    unsigned short cw;
    __asm__ volatile("fnstcw %0" : "=m"(cw));
    return cw;
}

/* x87 precision control to 53 bits, round to nearest, all exceptions masked: Java's double. */
static void fpcw_set(void)
{
    unsigned short cw = 0x027F;
    __asm__ volatile("fldcw %0" : : "m"(cw));
}

static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

/* Stage counters: chunks with a difference, and the first difference seen. */
typedef struct {
    const char *name;
    int bad;
    char first[256];
} stage;

static void cmp_doubles(stage *s, int cx, int cz, const char *what, const uint8_t *want, const double *got, int n)
{
    for (int i = 0; i < n; i++) {
        uint64_t a, b;
        memcpy(&a, want + 8 * i, 8);
        memcpy(&b, &got[i], 8);
        if (a != b) {
            if (s->bad++ == 0)
                snprintf(s->first, sizeof s->first, "chunk %d,%d %s[%d] captured %.17g (%016llx) got %.17g (%016llx)",
                         cx, cz, what, i, rdf64(want + 8 * i), (unsigned long long)a, got[i], (unsigned long long)b);
            return;
        }
    }
}

static void cmp_ids(stage *s, int cx, int cz, const char *what, const uint8_t *want, const uint16_t *got, int n)
{
    for (int i = 0; i < n; i++)
        if (rd16(want + 2 * i) != got[i]) {
            if (s->bad++ == 0)
                snprintf(s->first, sizeof s->first, "chunk %d,%d %s[%d] captured %u got %u", cx, cz, what, i,
                         rd16(want + 2 * i), got[i]);
            return;
        }
}

static void cmp_blocks(stage *s, int cx, int cz, const uint8_t *want, const uint16_t *got)
{
    for (int i = 0; i < 65536; i++) {
        if (rd16(want + 2 * i) != got[i]) {
            if (s->bad++ == 0)
                snprintf(s->first, sizeof s->first, "chunk %d,%d block x%d z%d y%d captured %u got %u", cx, cz,
                         i >> 12, (i >> 8) & 15, i & 255, rd16(want + 2 * i), got[i]);
            return;
        }
    }
}

static void *worker(void *arg)
{
    long t = (long)arg;
    fpcw_set();
    double o0[25], o1[825], o2[825], o3[825], field[825];
    static __thread uint16_t blocks[65536];
    for (int r = 0; r < rounds; r++)
        for (int i = (int)t; i < njobs; i += nthreads) {
#ifdef REPLAY_VPU
            if (use_vector) {
                noise_octaves(&W.gen[0], o0, jobs[i].cx * 4, 10, jobs[i].cz * 4, 5, 1, 5, 200.0, 1.0, 200.0);
                vshape_octaves3d(&W, jobs[i].cx, jobs[i].cz, o1, o2, o3);
            } else
#endif
                shape_octaves(&W, jobs[i].cx, jobs[i].cz, o0, o1, o2, o3);
            shape_combine(&W, jobs[i].bi10, o0, o1, o2, o3, field);
            shape_interp(field, blocks);
        }
    return NULL;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: replay DIR [THREADS [ROUNDS]]\n"); return 2; }
    if (argc > 2) nthreads = atoi(argv[2]);
    if (argc > 3) rounds = atoi(argv[3]);
    if (argc > 4) use_vector = strcmp(argv[4], "vector") == 0;
    unsigned short cw0 = fpcw_get();
    fpcw_set();
    printf("x87 control word: 0x%04x at start, 0x%04x after fldcw\n", cw0, fpcw_get());

    char path[4096];
    snprintf(path, sizeof path, "%s/world.bin", argv[1]);
    capfile wf;
    if (cap_load(&wf, path, "PWGW") || world_load(&W, &wf, path)) return 1;
    if (gl_load(&G, &wf, path)) return 1;
    static gl_arena arena;
    if (gl_arena_init(&arena, 1 << 18)) return 1;
    if (surf_load(&S, &wf, path)) return 1;
    printf("surface: %s\n", S.have ? "SURB, SNOI, BLKS present" : "no surface tables (the surface stage is skipped)");
    printf("layers: %s\n", G.have ? "LAYR present" : "no LAYR (the biome stages are skipped)");
    printf("world: octaves %d %d %d %d, amplified %d\n", W.gen[0].n, W.gen[1].n, W.gen[2].n, W.gen[3].n,
           W.amplified);

    DIR *d = opendir(argv[1]);
    if (!d) { perror(argv[1]); return 1; }
    stage st[8] = { { "octaves", 0, "" }, { "combine (from captured octaves)", 0, "" },
                    { "interpolation (from captured field)", 0, "" }, { "shape end to end", 0, "" },
                    { "biomes 10x10 from coordinates", 0, "" }, { "biomes 16x16 from coordinates", 0, "" },
                    { "3D octaves on the vector unit", 0, "" }, { "surface (from captured shape)", 0, "" } };
    int cap = 64;
    jobs = malloc(sizeof(job) * cap);
    struct dirent *e;
    static double o0[25], o1[825], o2[825], o3[825], field[825];
    static uint16_t blocks[65536];
    while ((e = readdir(d))) {
        if (strncmp(e->d_name, "c_", 2) != 0) continue;
        snprintf(path, sizeof path, "%s/%s", argv[1], e->d_name);
        capfile cf;
        if (cap_load(&cf, path, "PWGC")) return 1;
        const uint8_t *pos = cap_need(&cf, path, "CHNK", 8);
        int cx = rd32(pos), cz = rd32(pos + 4);
        const uint8_t *bi = cap_need(&cf, path, "BI10", 200);
        const uint8_t *c0 = cap_need(&cf, path, "OCT0", 200), *c1 = cap_need(&cf, path, "OCT1", 6600);
        const uint8_t *c2 = cap_need(&cf, path, "OCT2", 6600), *c3 = cap_need(&cf, path, "OCT3", 6600);
        const uint8_t *cf_field = cap_need(&cf, path, "FLD ", 6600);
        const uint8_t *shap = cap_need(&cf, path, "SHAP", 131072);

        if (njobs == cap) jobs = realloc(jobs, sizeof(job) * (cap *= 2));
        jobs[njobs].cx = cx;
        jobs[njobs].cz = cz;
        for (int i = 0; i < 100; i++) jobs[njobs].bi10[i] = rd16(bi + 2 * i);

        /* each stage from the captured inputs, so a difference names its stage */
        shape_octaves(&W, cx, cz, o0, o1, o2, o3);
        int before = st[0].bad;
        cmp_doubles(&st[0], cx, cz, "OCT0", c0, o0, 25);
        if (st[0].bad == before) cmp_doubles(&st[0], cx, cz, "OCT1", c1, o1, 825);
        if (st[0].bad == before) cmp_doubles(&st[0], cx, cz, "OCT2", c2, o2, 825);
        if (st[0].bad == before) cmp_doubles(&st[0], cx, cz, "OCT3", c3, o3, 825);

        double k0[25], k1[825], k2[825], k3[825], kf[825];
        for (int i = 0; i < 25; i++) k0[i] = rdf64(c0 + 8 * i);
        for (int i = 0; i < 825; i++) {
            k1[i] = rdf64(c1 + 8 * i);
            k2[i] = rdf64(c2 + 8 * i);
            k3[i] = rdf64(c3 + 8 * i);
            kf[i] = rdf64(cf_field + 8 * i);
        }
        shape_combine(&W, jobs[njobs].bi10, k0, k1, k2, k3, field);
        cmp_doubles(&st[1], cx, cz, "FLD", cf_field, field, 825);
        shape_interp(kf, blocks);
        cmp_blocks(&st[2], cx, cz, shap, blocks);

        shape_combine(&W, jobs[njobs].bi10, o0, o1, o2, o3, field);
        shape_interp(field, blocks);
        cmp_blocks(&st[3], cx, cz, shap, blocks);
        if (G.have) {
            uint16_t b10[100], b16[256];
            gl_biomes10(&G, &arena, cx, cz, b10);
            cmp_ids(&st[4], cx, cz, "BI10", bi, b10, 100);
            gl_biomes16(&G, &arena, cx, cz, b16);
            cmp_ids(&st[5], cx, cz, "BI16", cap_need(&cf, path, "BI16", 512), b16, 256);
            uint32_t fl = 0, sl;
            const uint8_t *fg = cap_section(&cf, "FLAG", &fl);
            const uint8_t *ssta = cap_section(&cf, "SSTA", &sl);
            if (S.have && ssta && !(fg && fl >= 4 && (rd32(fg) & 1))) {
                uint16_t s16[256];
                gl_slots16(&G, &arena, cx, cz, s16);
                static uint16_t sb[65536];
                static uint8_t sm[65536];
                for (int i = 0; i < 65536; i++) sb[i] = rd16(shap + 2 * i);
                memset(sm, 0, sizeof sm);
                surf_state es[256];
                int ne = surf_chunk(&S, cx, cz, s16, sb, sm, es);
                if (ne < 0) surf_skipped++;
                else {
                    int b7 = st[7].bad;
                    cmp_blocks(&st[7], cx, cz, cap_need(&cf, path, "SURF", 131072), sb);
                    const uint8_t *meta = cap_need(&cf, path, "META", 65536);
                    for (int i = 0; st[7].bad == b7 && i < 65536; i++)
                        if (meta[i] != sm[i] && st[7].bad++ == 0)
                            snprintf(st[7].first, sizeof st[7].first, "chunk %d,%d META[%d] captured %u got %u", cx, cz, i, meta[i], sm[i]);
                    /* every end state the card reports must equal the captured object fields */
                    int nc = rd32(ssta);
                    surf_states += ne;
                    for (int e2 = 0; st[7].bad == b7 && e2 < ne; e2++) {
                        int found = 0;
                        for (int c = 0; c < nc; c++) {
                            const uint8_t *q = ssta + 4 + 8 * c;
                            if (rd16(q) != es[e2].slot) continue;
                            found = 1;
                            if (rd16(q + 2) != es[e2].top || rd16(q + 4) != es[e2].filler || q[6] != es[e2].meta) {
                                if (st[7].bad++ == 0)
                                    snprintf(st[7].first, sizeof st[7].first, "chunk %d,%d end state of slot %d: captured %u %u %u got %u %u %u", cx, cz,
                                             es[e2].slot, rd16(q + 2), rd16(q + 4), q[6], es[e2].top, es[e2].filler, es[e2].meta);
                            }
                        }
                        if (!found && st[7].bad++ == 0)
                            snprintf(st[7].first, sizeof st[7].first, "chunk %d,%d end state of slot %d not captured", cx, cz, es[e2].slot);
                    }
                }
            }
        }
#ifdef REPLAY_VPU
        {
            static double v1[825], v2[825], v3[825];
            vshape_octaves3d(&W, cx, cz, v1, v2, v3);
            int b4 = st[6].bad;
            cmp_doubles(&st[6], cx, cz, "OCT1", c1, v1, 825);
            if (st[6].bad == b4) cmp_doubles(&st[6], cx, cz, "OCT2", c2, v2, 825);
            if (st[6].bad == b4) cmp_doubles(&st[6], cx, cz, "OCT3", c3, v3, 825);
        }
#endif
        njobs++;
        cap_free(&cf);
    }
    closedir(d);

    int all = 0;
#ifdef REPLAY_VPU
    int nst = 8;
#else
    int nst = 8;
#endif
    for (int s = 0; s < nst; s++) {
        if (!G.have && (s == 4 || s == 5)) continue;
#ifndef REPLAY_VPU
        if (s == 6) continue;
#endif
        if (s == 7 && !S.have) continue;
        printf("%-38s %d of %d chunks differ%s%s\n", st[s].name, st[s].bad, njobs, st[s].bad ? ": first " : "",
               st[s].first);
        all += st[s].bad;
    }
    if (S.have) printf("surface: %d chunks skipped (a biome whose surface is not ported, e.g. Mesa), %d end states checked\n", surf_skipped, surf_states);

    pthread_t th[1024];
    if (nthreads > 1024) nthreads = 1024;
    double t0 = now();
    for (long t = 0; t < nthreads; t++) pthread_create(&th[t], NULL, worker, (void *)t);
    for (int t = 0; t < nthreads; t++) pthread_join(th[t], NULL);
    double s = now() - t0;
    printf("timing (%s octaves): %d chunks x %d rounds on %d threads in %.3f s: %.1f chunks/s, %.3f ms a chunk a thread\n",
           use_vector ? "vector" : "scalar", njobs,
           rounds, nthreads, s, njobs * rounds / s, 1e3 * s * nthreads / (njobs * rounds));
    return all ? 1 : 0;
}
