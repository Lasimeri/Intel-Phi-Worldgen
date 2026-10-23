/* rwgreplay.c: replays an RWG capture (/phiwg rwgcap, docs/rwg-capture-format.md) through lib/rwg.c and compares
 * each chunk's heights, chosen biomes, river strengths and mapGenBiomes with what RWG made, bit for bit.
 *   rwgreplay DIR [threads] [rounds]
 * Prints the chunks that differ (first column and values), per-stage counts, and the time per chunk a thread.
 * threads 0: hashes of each piece (noise, cell noise, biome choice, ocean, river) on a grid, for host against card.
 * See rwgreplay.md. */
#include "capfile.h"
#include "rwg.h"

#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    int cx, cz;
    rwg_heights want;
} chunk;

static rwg_world W;
static chunk *chunks;
static int nchunks, rounds = 1;
static int next_chunk;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static long bad_height, bad_biome, bad_river, bad_mapgen, bad_chunks;
static double busy;

static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

static int same(float a, float b) { return memcmp(&a, &b, 4) == 0; }

static void fpcw53(void)
{
#if defined(__x86_64__) && !defined(__SSE2__)
    /* x87 precision control 53 bits, round to nearest (pwgd.c fpcw_set) */
    unsigned short cw = 0x027F;
    __asm__ volatile("fldcw %0" : : "m"(cw));
#endif
}

static void check(const chunk *c, const rwg_heights *got)
{
    int h = 0, b = 0, r = 0, m = 0, first = -1;
    for (int i = 0; i < 256; i++) {
        if (!same(got->height[i], c->want.height[i])) h++, first = first < 0 ? i : first;
        if (got->biome[i] != c->want.biome[i]) b++;
        if (!same(got->river[i], c->want.river[i])) r++;
    }
    for (int i = 0; i < 258; i++)
        if (!same(got->mapgen[i], c->want.mapgen[i])) m++;
    if (!(h | b | r | m)) return;
    pthread_mutex_lock(&lock);
    bad_chunks++;
    bad_height += h, bad_biome += b, bad_river += r, bad_mapgen += m;
    if (bad_chunks <= 12) {
        printf("chunk %d,%d: heights %d, biomes %d, rivers %d, mapgen %d differ", c->cx, c->cz, h, b, r, m);
        if (first >= 0)
            printf("; column %d,%d height %.9g want %.9g, biome %d want %d", first / 16, first % 16, got->height[first],
                   c->want.height[first], got->biome[(first % 16) * 16 + first / 16],
                   c->want.biome[(first % 16) * 16 + first / 16]);
        printf("\n");
    }
    pthread_mutex_unlock(&lock);
}

static void *work(void *arg)
{
    (void)arg;
    fpcw53();
    rwg_scratch *s = malloc(sizeof *s);
    rwg_heights got;
    double t = 0;
    for (;;) {
        pthread_mutex_lock(&lock);
        int k = next_chunk++;
        pthread_mutex_unlock(&lock);
        if (k >= nchunks * rounds) break;
        const chunk *c = &chunks[k % nchunks];
        double t0 = now();
        rwg_new_noise(&W, s, c->cx * 16, c->cz * 16, &got);
        t += now() - t0;
        if (k < nchunks) check(c, &got);
    }
    pthread_mutex_lock(&lock);
    busy += t;
    pthread_mutex_unlock(&lock);
    free(s);
    return NULL;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: rwgreplay DIR [threads] [rounds]\n");
        return 2;
    }
    int threads = argc > 2 ? atoi(argv[2]) : 1;
    rounds = argc > 3 ? atoi(argv[3]) : 1;
    char path[4096];
    capfile f;
    snprintf(path, sizeof path, "%s/rwg-world.bin", argv[1]);
    if (cap_load(&f, path, "RWGW") || rwg_load(&W, &f, path)) return 1;
    cap_free(&f);
    DIR *d = opendir(argv[1]);
    if (!d) return perror(argv[1]), 1;
    struct dirent *e;
    int cap = 0;
    while ((e = readdir(d))) {
        if (strncmp(e->d_name, "h_", 2)) continue;
        if (nchunks == cap) chunks = realloc(chunks, (cap = cap ? cap * 2 : 256) * sizeof *chunks);
        chunk *c = &chunks[nchunks];
        snprintf(path, sizeof path, "%s/%s", argv[1], e->d_name);
        uint32_t n;
        if (cap_load(&f, path, "PWGH")) return 1;
        const uint8_t *q = cap_need(&f, path, "CHNK", 8);
        c->cx = rd32(q), c->cz = rd32(q + 4);
        q = cap_need(&f, path, "HGTS", 1024);
        memcpy(c->want.height, q, 1024);
        q = cap_need(&f, path, "RBIO", 512);
        memcpy(c->want.biome, q, 512);
        q = cap_need(&f, path, "RIVR", 1024);
        memcpy(c->want.river, q, 1024);
        q = cap_section(&f, "MGBI", &n);
        if (!q || n != 258 * 4) return fprintf(stderr, "%s: MGBI is %u bytes\n", path, q ? n : 0), 1;
        memcpy(c->want.mapgen, q, n);
        cap_free(&f);
        nchunks++;
    }
    closedir(d);
    if (threads == 0) {
        /* the pieces on a grid, as hashes of their bits: compare the host's output with the card's */
        fpcw53();
        uint64_t h[7] = { 0 };
        for (int x = -2000; x < 2000; x += 37)
            for (int z = -2000; z < 2000; z += 41) {
                float v[7] = { rwg_noise1(&W.perlin, x / 7.3f), rwg_noise2(&W.perlin, x / 7.3f, z / 3.1f),
                               rwg_cell_noise(&W.biomecell, x / 450.0, z / 450.0, 1.0), rwg_cell_border(&W.cmr_cell, x / 1250.0, z / 1250.0, 0.16666666666666666, 1.0f),
                               (float)rwg_biome_at(&W, x, z), rwg_ocean(&W, x, z), rwg_river_strength(&W, x, z) };
                for (int i = 0; i < 7; i++) { uint32_t u; memcpy(&u, &v[i], 4); h[i] = h[i] * 1000003u + u; }
            }
        printf("noise1 %016llx noise2 %016llx cellnoise %016llx border %016llx biome %016llx ocean %016llx river %016llx\n",
               (unsigned long long)h[0], (unsigned long long)h[1], (unsigned long long)h[2], (unsigned long long)h[3], (unsigned long long)h[4], (unsigned long long)h[5], (unsigned long long)h[6]);
        float a = rwg_noise2(&W.perlin, 10.25f, -3.5f); uint32_t ua; memcpy(&ua, &a, 4);
        printf("noise2(10.25, -3.5) = %08x %.9g\n", ua, a);
        return 0;
    }
    pthread_t t[512];
    if (threads < 1 || threads > 512) threads = 1; /* 0 above: the self-test */
    double t0 = now();
    for (int i = 0; i < threads; i++) pthread_create(&t[i], NULL, work, NULL);
    for (int i = 0; i < threads; i++) pthread_join(t[i], NULL);
    double wall = now() - t0;
    printf("%s: %d chunks, %d realistic biomes, %d threads x %d rounds: %ld chunks differ (heights %ld, biomes %ld, "
           "rivers %ld, mapgen %ld); %.3f ms a chunk a thread, %.1f chunks/s\n",
           argv[1], nchunks, W.nbiomes, threads, rounds, bad_chunks, bad_height, bad_biome, bad_river, bad_mapgen,
           busy * 1e3 / (nchunks * rounds), nchunks * rounds / wall);
    return bad_chunks != 0;
}
