/* genlayer.c: the overworld's GenLayer biome chains, ported from 1.7.10 + Forge 10.13.4.1614. See genlayer.md. */
#include "genlayer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* vanilla biome ids (BiomeGenBase's static fields) */
enum {
    B_OCEAN = 0, B_PLAINS = 1, B_DESERT = 2, B_EXTREME_HILLS = 3, B_FOREST = 4, B_TAIGA = 5, B_SWAMPLAND = 6,
    B_RIVER = 7, B_FROZEN_RIVER = 11, B_ICE_PLAINS = 12, B_ICE_MOUNTAINS = 13, B_MUSHROOM_ISLAND = 14,
    B_MUSHROOM_SHORE = 15, B_BEACH = 16, B_DESERT_HILLS = 17, B_FOREST_HILLS = 18, B_TAIGA_HILLS = 19,
    B_EXTREME_HILLS_EDGE = 20, B_JUNGLE = 21, B_JUNGLE_HILLS = 22, B_JUNGLE_EDGE = 23, B_DEEP_OCEAN = 24,
    B_STONE_BEACH = 25, B_COLD_BEACH = 26, B_BIRCH_FOREST = 27, B_BIRCH_FOREST_HILLS = 28, B_ROOFED_FOREST = 29,
    B_COLD_TAIGA = 30, B_COLD_TAIGA_HILLS = 31, B_MEGA_TAIGA = 32, B_MEGA_TAIGA_HILLS = 33,
    B_EXTREME_HILLS_PLUS = 34, B_SAVANNA = 35, B_SAVANNA_PLATEAU = 36, B_MESA = 37, B_MESA_PLATEAU_F = 38,
    B_MESA_PLATEAU = 39
};
enum { T_MEDIUM = 2 }; /* BiomeGenBase.TempCategory: OCEAN, COLD, MEDIUM, WARM */
enum { F_SNOW = 1, F_MESA = 2, F_OCEANIC = 4, F_JUNGLE = 8 };

/* ---- the LCG (GenLayer's long arithmetic, wrapping) ---- */

static inline int64_t lcg(int64_t s)
{
    return (int64_t)((uint64_t)s * ((uint64_t)s * 6364136223846793005ULL + 1442695040888963407ULL));
}
static inline int64_t add64(int64_t a, int64_t b) { return (int64_t)((uint64_t)a + (uint64_t)b); }

typedef struct {
    int64_t cs, wgs;
} rng;

static inline void init_chunk(rng *r, int64_t x, int64_t z)
{
    r->cs = r->wgs;
    r->cs = add64(lcg(r->cs), x);
    r->cs = add64(lcg(r->cs), z);
    r->cs = add64(lcg(r->cs), x);
    r->cs = add64(lcg(r->cs), z);
}

static inline int next_int(rng *r, int n)
{
    int j = (int)((r->cs >> 24) % (int64_t)n);
    if (j < 0) j += n;
    r->cs = add64(lcg(r->cs), r->wgs);
    return j;
}

static inline int select2(rng *r, int a, int b) { return next_int(r, 2) == 0 ? a : b; }
static inline int select4(rng *r, int a, int b, int c, int d)
{
    int v[4] = { a, b, c, d };
    return v[next_int(r, 4)];
}

static int mode_or_random(rng *r, int a, int b, int c, int d)
{
    return b == c && c == d ? b
         : a == b && a == c ? a
         : a == b && a == d ? a
         : a == c && a == d ? a
         : a == b && c != d ? a
         : a == c && b != d ? a
         : a == d && b != c ? a
         : b == c && a != d ? b
         : b == d && a != c ? b
         : c == d && a != b ? c
         : select4(r, a, b, c, d);
}

/* ---- biome traits (BiomeGenBase.getBiome and friends) ---- */

/* getBiome(id) as an id: -1 for null; out of range is ocean (with a warning, in Java) */
static inline int get_biome(const gl_world *g, int id)
{
    if (id >= 0 && id < 256) return g->present[id] ? id : -1;
    return B_OCEAN;
}

static int compare_biomes(const gl_world *g, int a, int b)
{
    if (a == b) return 1;
    if (a != B_MESA_PLATEAU_F && a != B_MESA_PLATEAU) {
        int pa = get_biome(g, a), pb = get_biome(g, b);
        if (pa < 0 || pb < 0) return 0;
        return pa == pb || g->cls[pa] == g->cls[pb];
    }
    return b == B_MESA_PLATEAU_F || b == B_MESA_PLATEAU;
}

static inline int oceanic(const gl_world *g, int id)
{
    int p = get_biome(g, id);
    return p >= 0 && (g->flags[p] & F_OCEANIC);
}

/* ---- arena ---- */

int gl_arena_init(gl_arena *a, int ints)
{
    a->mem = malloc(sizeof(int32_t) * (size_t)ints);
    a->cap = ints;
    a->used = 0;
    return a->mem ? 0 : -1;
}
void gl_arena_free(gl_arena *a) { free(a->mem); }

static int32_t *take(gl_arena *a, int n)
{
    if (a->used + n > a->cap) {
        fprintf(stderr, "genlayer: arena of %d ints too small\n", a->cap);
        abort();
    }
    int32_t *p = a->mem + a->used;
    a->used += n;
    return p;
}

/* ---- the layers: each is its class's getInts ---- */

static void ints(const gl_world *g, gl_arena *a, int li, int x, int z, int w, int h, int32_t *out);

#define AT(arr, i, j, width) (arr)[(i) + (j) * (width)]

static void island(const gl_layer *L, int x, int z, int w, int h, int32_t *out)
{
    rng r = { 0, L->wgs };
    for (int i1 = 0; i1 < h; ++i1)
        for (int j1 = 0; j1 < w; ++j1) {
            init_chunk(&r, x + j1, z + i1);
            out[j1 + i1 * w] = next_int(&r, 10) == 0 ? 1 : 0;
        }
    if (x > -w && x <= 0 && z > -h && z <= 0) out[-x + -z * w] = 1;
}

static void zoom(const gl_world *g, gl_arena *a, const gl_layer *L, int fuzzy, int x, int z, int w, int h,
                 int32_t *out)
{
    int i1 = x >> 1, j1 = z >> 1, k1 = (w >> 1) + 2, l1 = (h >> 1) + 2;
    int32_t *p = take(a, k1 * l1);
    ints(g, a, L->in0, i1, j1, k1, l1, p);
    int i2 = (k1 - 1) << 1, j2 = (l1 - 1) << 1;
    int32_t *q = take(a, i2 * j2);
    rng r = { 0, L->wgs };
    for (int k2 = 0; k2 < l1 - 1; ++k2) {
        int l2 = (k2 << 1) * i2;
        int i3 = 0;
        int j3 = p[i3 + 0 + (k2 + 0) * k1];
        int k3 = p[i3 + 0 + (k2 + 1) * k1];
        for (; i3 < k1 - 1; ++i3) {
            init_chunk(&r, (int64_t)((i3 + i1) << 1), (int64_t)((k2 + j1) << 1));
            int l3 = p[i3 + 1 + (k2 + 0) * k1];
            int i4 = p[i3 + 1 + (k2 + 1) * k1];
            q[l2] = j3;
            int idx = l2 + i2;
            l2++;
            q[idx] = select2(&r, j3, k3);
            q[l2] = select2(&r, j3, l3);
            idx = l2 + i2;
            l2++;
            q[idx] = fuzzy ? select4(&r, j3, l3, k3, i4) : mode_or_random(&r, j3, l3, k3, i4);
            j3 = l3;
            k3 = i4;
        }
    }
    for (int l2 = 0; l2 < h; ++l2) memcpy(out + l2 * w, q + (l2 + (z & 1)) * i2 + (x & 1), sizeof(int32_t) * (size_t)w);
}

static void add_island(const gl_world *g, gl_arena *a, const gl_layer *L, int x, int z, int w, int h, int32_t *out)
{
    int k1 = w + 2, l1 = h + 2;
    int32_t *p = take(a, k1 * l1);
    ints(g, a, L->in0, x - 1, z - 1, k1, l1, p);
    rng r = { 0, L->wgs };
    for (int i2 = 0; i2 < h; ++i2)
        for (int j2 = 0; j2 < w; ++j2) {
            int k2 = p[j2 + 0 + (i2 + 0) * k1];
            int l2 = p[j2 + 2 + (i2 + 0) * k1];
            int i3 = p[j2 + 0 + (i2 + 2) * k1];
            int j3 = p[j2 + 2 + (i2 + 2) * k1];
            int k3 = p[j2 + 1 + (i2 + 1) * k1];
            init_chunk(&r, j2 + x, i2 + z);
            int v;
            if (k3 == 0 && (k2 != 0 || l2 != 0 || i3 != 0 || j3 != 0)) {
                int l3 = 1, i4 = 1;
                if (k2 != 0 && next_int(&r, l3++) == 0) i4 = k2;
                if (l2 != 0 && next_int(&r, l3++) == 0) i4 = l2;
                if (i3 != 0 && next_int(&r, l3++) == 0) i4 = i3;
                if (j3 != 0 && next_int(&r, l3++) == 0) i4 = j3;
                if (next_int(&r, 3) == 0) v = i4;
                else if (i4 == 4) v = 4;
                else v = 0;
            } else if (k3 > 0 && (k2 == 0 || l2 == 0 || i3 == 0 || j3 == 0)) {
                if (next_int(&r, 5) == 0) v = k3 == 4 ? 4 : 0;
                else v = k3;
            } else v = k3;
            out[j2 + i2 * w] = v;
        }
}

static void remove_ocean(const gl_world *g, gl_arena *a, const gl_layer *L, int x, int z, int w, int h,
                         int32_t *out)
{
    int k1 = w + 2, l1 = h + 2;
    int32_t *p = take(a, k1 * l1);
    ints(g, a, L->in0, x - 1, z - 1, k1, l1, p);
    rng r = { 0, L->wgs };
    for (int i2 = 0; i2 < h; ++i2)
        for (int j2 = 0; j2 < w; ++j2) {
            int k2 = p[j2 + 1 + (i2 + 1 - 1) * (w + 2)];
            int l2 = p[j2 + 1 + 1 + (i2 + 1) * (w + 2)];
            int i3 = p[j2 + 1 - 1 + (i2 + 1) * (w + 2)];
            int j3 = p[j2 + 1 + (i2 + 1 + 1) * (w + 2)];
            int k3 = p[j2 + 1 + (i2 + 1) * k1];
            out[j2 + i2 * w] = k3;
            init_chunk(&r, j2 + x, i2 + z);
            if (k3 == 0 && k2 == 0 && l2 == 0 && i3 == 0 && j3 == 0 && next_int(&r, 2) == 0) out[j2 + i2 * w] = 1;
        }
}

static void add_snow(const gl_world *g, gl_arena *a, const gl_layer *L, int x, int z, int w, int h, int32_t *out)
{
    int k1 = w + 2, l1 = h + 2;
    int32_t *p = take(a, k1 * l1);
    ints(g, a, L->in0, x - 1, z - 1, k1, l1, p);
    rng r = { 0, L->wgs };
    for (int i2 = 0; i2 < h; ++i2)
        for (int j2 = 0; j2 < w; ++j2) {
            int k2 = p[j2 + 1 + (i2 + 1) * k1];
            init_chunk(&r, j2 + x, i2 + z);
            if (k2 == 0) out[j2 + i2 * w] = 0;
            else {
                int l2 = next_int(&r, 6);
                out[j2 + i2 * w] = l2 == 0 ? 4 : l2 <= 1 ? 3 : 1;
            }
        }
}

static void edge(const gl_world *g, gl_arena *a, const gl_layer *L, int x, int z, int w, int h, int32_t *out)
{
    rng r = { 0, L->wgs };
    if (L->mode == 2) { /* SPECIAL */
        int32_t *p = take(a, w * h);
        ints(g, a, L->in0, x, z, w, h, p);
        for (int i1 = 0; i1 < h; ++i1)
            for (int j1 = 0; j1 < w; ++j1) {
                init_chunk(&r, j1 + x, i1 + z);
                int k1 = p[j1 + i1 * w];
                if (k1 != 0 && next_int(&r, 13) == 0) k1 |= (1 + next_int(&r, 15)) << 8 & 3840;
                out[j1 + i1 * w] = k1;
            }
        return;
    }
    int k1 = 1 + w + 1, l1 = 1 + h + 1;
    int32_t *p = take(a, k1 * l1);
    ints(g, a, L->in0, x - 1, z - 1, k1, l1, p);
    for (int i2 = 0; i2 < h; ++i2)
        for (int j2 = 0; j2 < w; ++j2) {
            if (L->mode == 0) init_chunk(&r, j2 + x, i2 + z); /* COOL_WARM seeds (and draws nothing) */
            int k2 = p[j2 + 1 + (i2 + 1) * k1];
            int l2 = p[j2 + 1 + (i2 + 1 - 1) * k1];
            int i3 = p[j2 + 1 + 1 + (i2 + 1) * k1];
            int j3 = p[j2 + 1 - 1 + (i2 + 1) * k1];
            int k3 = p[j2 + 1 + (i2 + 1 + 1) * k1];
            if (L->mode == 0 && k2 == 1) {
                int f = l2 == 3 || i3 == 3 || j3 == 3 || k3 == 3;
                int f1 = l2 == 4 || i3 == 4 || j3 == 4 || k3 == 4;
                if (f || f1) k2 = 2;
            } else if (L->mode == 1 && k2 == 4) {
                int f = l2 == 2 || i3 == 2 || j3 == 2 || k3 == 2;
                int f1 = l2 == 1 || i3 == 1 || j3 == 1 || k3 == 1;
                if (f1 || f) k2 = 3;
            }
            out[j2 + i2 * w] = k2;
        }
}

static void add_mushroom(const gl_world *g, gl_arena *a, const gl_layer *L, int x, int z, int w, int h,
                         int32_t *out)
{
    int k1 = w + 2, l1 = h + 2;
    int32_t *p = take(a, k1 * l1);
    ints(g, a, L->in0, x - 1, z - 1, k1, l1, p);
    rng r = { 0, L->wgs };
    for (int i2 = 0; i2 < h; ++i2)
        for (int j2 = 0; j2 < w; ++j2) {
            int k2 = p[j2 + 0 + (i2 + 0) * k1];
            int l2 = p[j2 + 2 + (i2 + 0) * k1];
            int i3 = p[j2 + 0 + (i2 + 2) * k1];
            int j3 = p[j2 + 2 + (i2 + 2) * k1];
            int k3 = p[j2 + 1 + (i2 + 1) * k1];
            init_chunk(&r, j2 + x, i2 + z);
            if (k3 == 0 && k2 == 0 && l2 == 0 && i3 == 0 && j3 == 0 && next_int(&r, 100) == 0)
                out[j2 + i2 * w] = B_MUSHROOM_ISLAND;
            else out[j2 + i2 * w] = k3;
        }
}

static void deep_ocean(const gl_world *g, gl_arena *a, const gl_layer *L, int x, int z, int w, int h, int32_t *out)
{
    int k1 = w + 2, l1 = h + 2;
    int32_t *p = take(a, k1 * l1);
    ints(g, a, L->in0, x - 1, z - 1, k1, l1, p);
    for (int i2 = 0; i2 < h; ++i2)
        for (int j2 = 0; j2 < w; ++j2) {
            int k2 = p[j2 + 1 + (i2 + 1 - 1) * (w + 2)];
            int l2 = p[j2 + 1 + 1 + (i2 + 1) * (w + 2)];
            int i3 = p[j2 + 1 - 1 + (i2 + 1) * (w + 2)];
            int j3 = p[j2 + 1 + (i2 + 1 + 1) * (w + 2)];
            int k3 = p[j2 + 1 + (i2 + 1) * k1];
            int l3 = (k2 == 0) + (l2 == 0) + (i3 == 0) + (j3 == 0);
            out[j2 + i2 * w] = k3 == 0 && l3 > 3 ? B_DEEP_OCEAN : k3;
        }
}

static void river_init(const gl_world *g, gl_arena *a, const gl_layer *L, int x, int z, int w, int h, int32_t *out)
{
    int32_t *p = take(a, w * h);
    ints(g, a, L->in0, x, z, w, h, p);
    rng r = { 0, L->wgs };
    for (int i1 = 0; i1 < h; ++i1)
        for (int j1 = 0; j1 < w; ++j1) {
            init_chunk(&r, j1 + x, i1 + z);
            out[j1 + i1 * w] = p[j1 + i1 * w] > 0 ? next_int(&r, 299999) + 2 : 0;
        }
}

/* GenLayerBiome.getWeightedBiomeEntry */
static int weighted(const gl_layer *L, rng *r, int type)
{
    int total = L->total[type];
    int weight = L->modded[type] ? next_int(r, total) : next_int(r, total / 10) * 10;
    for (int i = 0; i < L->n[type]; i++) {
        weight -= L->list[type][i].weight;
        if (weight < 0) return L->list[type][i].id;
    }
    return -1; /* getItem returned null: Java would throw on .biome; never seen */
}

static void biome(const gl_world *g, gl_arena *a, const gl_layer *L, int x, int z, int w, int h, int32_t *out)
{
    int32_t *p = take(a, w * h);
    ints(g, a, L->in0, x, z, w, h, p);
    rng r = { 0, L->wgs };
    for (int i1 = 0; i1 < h; ++i1)
        for (int j1 = 0; j1 < w; ++j1) {
            init_chunk(&r, j1 + x, i1 + z);
            int k1 = p[j1 + i1 * w];
            int l1 = (k1 & 3840) >> 8;
            k1 &= -3841;
            int v;
            if (oceanic(g, k1)) v = k1;
            else if (k1 == B_MUSHROOM_ISLAND) v = k1;
            else if (k1 == 1) v = l1 > 0 ? (next_int(&r, 3) == 0 ? B_MESA_PLATEAU : B_MESA_PLATEAU_F) : weighted(L, &r, 0);
            else if (k1 == 2) v = l1 > 0 ? B_JUNGLE : weighted(L, &r, 1);
            else if (k1 == 3) v = l1 > 0 ? B_MEGA_TAIGA : weighted(L, &r, 2);
            else if (k1 == 4) v = weighted(L, &r, 3);
            else v = B_MUSHROOM_ISLAND;
            out[j1 + i1 * w] = v;
        }
}

/* GenLayerBiomeEdge.func_151634_b */
static int edge_ok(const gl_world *g, int a, int b)
{
    if (compare_biomes(g, a, b)) return 1;
    int pa = get_biome(g, a), pb = get_biome(g, b);
    if (pa < 0 || pb < 0) return 0;
    int ta = g->temp[pa], tb = g->temp[pb];
    return ta == tb || ta == T_MEDIUM || tb == T_MEDIUM;
}

static void biome_edge(const gl_world *g, gl_arena *a, const gl_layer *L, int x, int z, int w, int h, int32_t *out)
{
    int ww = w + 2;
    int32_t *p = take(a, ww * (h + 2));
    ints(g, a, L->in0, x - 1, z - 1, w + 2, h + 2, p);
    rng r = { 0, L->wgs };
    for (int i1 = 0; i1 < h; ++i1)
        for (int j1 = 0; j1 < w; ++j1) {
            init_chunk(&r, j1 + x, i1 + z);
            int k1 = p[j1 + 1 + (i1 + 1) * ww];
            int n = p[j1 + 1 + (i1 + 1 - 1) * ww], e = p[j1 + 1 + 1 + (i1 + 1) * ww];
            int wv = p[j1 + 1 - 1 + (i1 + 1) * ww], s = p[j1 + 1 + (i1 + 1 + 1) * ww];
            int *o = &out[j1 + i1 * w];
            /* func_151636_a (extreme hills, by temperature) then func_151635_b three times (by class) */
            if (compare_biomes(g, k1, B_EXTREME_HILLS)) {
                *o = edge_ok(g, n, B_EXTREME_HILLS) && edge_ok(g, e, B_EXTREME_HILLS) && edge_ok(g, wv, B_EXTREME_HILLS) &&
                             edge_ok(g, s, B_EXTREME_HILLS)
                         ? k1
                         : B_EXTREME_HILLS_EDGE;
                continue;
            }
            static const int from[3] = { B_MESA_PLATEAU_F, B_MESA_PLATEAU, B_MEGA_TAIGA };
            static const int to[3] = { B_MESA, B_MESA, B_TAIGA };
            int done = 0;
            for (int t = 0; t < 3 && !done; t++)
                if (k1 == from[t]) {
                    *o = compare_biomes(g, n, from[t]) && compare_biomes(g, e, from[t]) && compare_biomes(g, wv, from[t]) &&
                                 compare_biomes(g, s, from[t])
                             ? k1
                             : to[t];
                    done = 1;
                }
            if (done) continue;
            if (k1 == B_DESERT) {
                *o = n != B_ICE_PLAINS && e != B_ICE_PLAINS && wv != B_ICE_PLAINS && s != B_ICE_PLAINS ? k1
                                                                                                    : B_EXTREME_HILLS_PLUS;
            } else if (k1 == B_SWAMPLAND) {
                if (n != B_DESERT && e != B_DESERT && wv != B_DESERT && s != B_DESERT && n != B_COLD_TAIGA &&
                    e != B_COLD_TAIGA && wv != B_COLD_TAIGA && s != B_COLD_TAIGA && n != B_ICE_PLAINS &&
                    e != B_ICE_PLAINS && wv != B_ICE_PLAINS && s != B_ICE_PLAINS)
                    *o = n != B_JUNGLE && s != B_JUNGLE && e != B_JUNGLE && wv != B_JUNGLE ? k1 : B_JUNGLE_EDGE;
                else *o = B_PLAINS;
            } else *o = k1;
        }
}

static void hills(const gl_world *g, gl_arena *a, const gl_layer *L, int x, int z, int w, int h, int32_t *out)
{
    int ww = w + 2;
    int32_t *p = take(a, ww * (h + 2));
    int32_t *q = take(a, ww * (h + 2));
    ints(g, a, L->in0, x - 1, z - 1, w + 2, h + 2, p);
    ints(g, a, L->in1, x - 1, z - 1, w + 2, h + 2, q);
    rng r = { 0, L->wgs };
    for (int i1 = 0; i1 < h; ++i1)
        for (int j1 = 0; j1 < w; ++j1) {
            init_chunk(&r, j1 + x, i1 + z);
            int k1 = p[j1 + 1 + (i1 + 1) * ww];
            int l1 = q[j1 + 1 + (i1 + 1) * ww];
            int flag = (l1 - 2) % 29 == 0;
            int *o = &out[j1 + i1 * w];
            if (k1 != 0 && l1 >= 2 && (l1 - 2) % 29 == 1 && k1 < 128) {
                *o = get_biome(g, k1 + 128) >= 0 ? k1 + 128 : k1;
            } else if (next_int(&r, 3) != 0 && !flag) {
                *o = k1;
            } else {
                int i2 = k1;
                if (k1 == B_DESERT) i2 = B_DESERT_HILLS;
                else if (k1 == B_FOREST) i2 = B_FOREST_HILLS;
                else if (k1 == B_BIRCH_FOREST) i2 = B_BIRCH_FOREST_HILLS;
                else if (k1 == B_ROOFED_FOREST) i2 = B_PLAINS;
                else if (k1 == B_TAIGA) i2 = B_TAIGA_HILLS;
                else if (k1 == B_MEGA_TAIGA) i2 = B_MEGA_TAIGA_HILLS;
                else if (k1 == B_COLD_TAIGA) i2 = B_COLD_TAIGA_HILLS;
                else if (k1 == B_PLAINS) i2 = next_int(&r, 3) == 0 ? B_FOREST_HILLS : B_FOREST;
                else if (k1 == B_ICE_PLAINS) i2 = B_ICE_MOUNTAINS;
                else if (k1 == B_JUNGLE) i2 = B_JUNGLE_HILLS;
                else if (k1 == B_OCEAN) i2 = B_DEEP_OCEAN;
                else if (k1 == B_EXTREME_HILLS) i2 = B_EXTREME_HILLS_PLUS;
                else if (k1 == B_SAVANNA) i2 = B_SAVANNA_PLATEAU;
                else if (compare_biomes(g, k1, B_MESA_PLATEAU_F)) i2 = B_MESA;
                else if (k1 == B_DEEP_OCEAN && next_int(&r, 3) == 0) i2 = next_int(&r, 2) == 0 ? B_PLAINS : B_FOREST;
                if (flag && i2 != k1) i2 = get_biome(g, i2 + 128) >= 0 ? i2 + 128 : k1;
                if (i2 == k1) *o = k1;
                else {
                    int j2 = p[j1 + 1 + (i1 + 1 - 1) * ww], k2 = p[j1 + 1 + 1 + (i1 + 1) * ww];
                    int l2 = p[j1 + 1 - 1 + (i1 + 1) * ww], i3 = p[j1 + 1 + (i1 + 1 + 1) * ww];
                    int j3 = compare_biomes(g, j2, k1) + compare_biomes(g, k2, k1) + compare_biomes(g, l2, k1) +
                             compare_biomes(g, i3, k1);
                    *o = j3 >= 3 ? i2 : k1;
                }
            }
        }
}

static inline int river_key(int v) { return v >= 2 ? 2 + (v & 1) : v; }

static void river(const gl_world *g, gl_arena *a, const gl_layer *L, int x, int z, int w, int h, int32_t *out)
{
    int k1 = w + 2, l1 = h + 2;
    int32_t *p = take(a, k1 * l1);
    ints(g, a, L->in0, x - 1, z - 1, k1, l1, p);
    for (int i2 = 0; i2 < h; ++i2)
        for (int j2 = 0; j2 < w; ++j2) {
            int k2 = river_key(p[j2 + 0 + (i2 + 1) * k1]);
            int l2 = river_key(p[j2 + 2 + (i2 + 1) * k1]);
            int i3 = river_key(p[j2 + 1 + (i2 + 0) * k1]);
            int j3 = river_key(p[j2 + 1 + (i2 + 2) * k1]);
            int k3 = river_key(p[j2 + 1 + (i2 + 1) * k1]);
            out[j2 + i2 * w] = k3 == k2 && k3 == i3 && k3 == l2 && k3 == j3 ? -1 : B_RIVER;
        }
}

static void smooth(const gl_world *g, gl_arena *a, const gl_layer *L, int x, int z, int w, int h, int32_t *out)
{
    int k1 = w + 2, l1 = h + 2;
    int32_t *p = take(a, k1 * l1);
    ints(g, a, L->in0, x - 1, z - 1, k1, l1, p);
    rng r = { 0, L->wgs };
    for (int i2 = 0; i2 < h; ++i2)
        for (int j2 = 0; j2 < w; ++j2) {
            int k2 = p[j2 + 0 + (i2 + 1) * k1];
            int l2 = p[j2 + 2 + (i2 + 1) * k1];
            int i3 = p[j2 + 1 + (i2 + 0) * k1];
            int j3 = p[j2 + 1 + (i2 + 2) * k1];
            int k3 = p[j2 + 1 + (i2 + 1) * k1];
            if (k2 == l2 && i3 == j3) {
                init_chunk(&r, j2 + x, i2 + z);
                k3 = next_int(&r, 2) == 0 ? k2 : i3;
            } else {
                if (k2 == l2) k3 = k2;
                if (i3 == j3) k3 = i3;
            }
            out[j2 + i2 * w] = k3;
        }
}

static void rare_biome(const gl_world *g, gl_arena *a, const gl_layer *L, int x, int z, int w, int h, int32_t *out)
{
    int32_t *p = take(a, (w + 2) * (h + 2));
    ints(g, a, L->in0, x - 1, z - 1, w + 2, h + 2, p);
    rng r = { 0, L->wgs };
    for (int i1 = 0; i1 < h; ++i1)
        for (int j1 = 0; j1 < w; ++j1) {
            init_chunk(&r, j1 + x, i1 + z);
            int k1 = p[j1 + 1 + (i1 + 1) * (w + 2)];
            out[j1 + i1 * w] = next_int(&r, 57) == 0 && k1 == B_PLAINS ? B_PLAINS + 128 : k1;
        }
}

/* GenLayerShore.func_151632_a */
static int shore_replace(const gl_world *g, const int32_t *p, int j1, int i1, int w, int k1, int with)
{
    if (oceanic(g, k1)) return k1;
    int a = p[j1 + 1 + (i1 + 1 - 1) * (w + 2)], b = p[j1 + 1 + 1 + (i1 + 1) * (w + 2)];
    int c = p[j1 + 1 - 1 + (i1 + 1) * (w + 2)], d = p[j1 + 1 + (i1 + 1 + 1) * (w + 2)];
    return !oceanic(g, a) && !oceanic(g, b) && !oceanic(g, c) && !oceanic(g, d) ? k1 : with;
}

/* GenLayerShore.func_151631_c */
static int jungle_ok(const gl_world *g, int id)
{
    int p = get_biome(g, id);
    if (p >= 0 && (g->flags[p] & F_JUNGLE)) return 1;
    return id == B_JUNGLE_EDGE || id == B_JUNGLE || id == B_JUNGLE_HILLS || id == B_FOREST || id == B_TAIGA ||
           oceanic(g, id);
}

static inline int is_mesa(const gl_world *g, int id)
{
    int p = get_biome(g, id);
    return p >= 0 && (g->flags[p] & F_MESA);
}

static void shore(const gl_world *g, gl_arena *a, const gl_layer *L, int x, int z, int w, int h, int32_t *out)
{
    int ww = w + 2;
    int32_t *p = take(a, ww * (h + 2));
    ints(g, a, L->in0, x - 1, z - 1, w + 2, h + 2, p);
    rng r = { 0, L->wgs };
    for (int i1 = 0; i1 < h; ++i1)
        for (int j1 = 0; j1 < w; ++j1) {
            init_chunk(&r, j1 + x, i1 + z);
            int k1 = p[j1 + 1 + (i1 + 1) * ww];
            int bg = get_biome(g, k1);
            int n = p[j1 + 1 + (i1 + 1 - 1) * ww], e = p[j1 + 1 + 1 + (i1 + 1) * ww];
            int wv = p[j1 + 1 - 1 + (i1 + 1) * ww], s = p[j1 + 1 + (i1 + 1 + 1) * ww];
            int *o = &out[j1 + i1 * w];
            if (k1 == B_MUSHROOM_ISLAND) {
                *o = n != B_OCEAN && e != B_OCEAN && wv != B_OCEAN && s != B_OCEAN ? k1 : B_MUSHROOM_SHORE;
            } else if (bg >= 0 && (g->flags[bg] & F_JUNGLE)) {
                if (jungle_ok(g, n) && jungle_ok(g, e) && jungle_ok(g, wv) && jungle_ok(g, s))
                    *o = !oceanic(g, n) && !oceanic(g, e) && !oceanic(g, wv) && !oceanic(g, s) ? k1 : B_BEACH;
                else *o = B_JUNGLE_EDGE;
            } else if (k1 != B_EXTREME_HILLS && k1 != B_EXTREME_HILLS_PLUS && k1 != B_EXTREME_HILLS_EDGE) {
                if (bg >= 0 && (g->flags[bg] & F_SNOW)) *o = shore_replace(g, p, j1, i1, w, k1, B_COLD_BEACH);
                else if (k1 != B_MESA && k1 != B_MESA_PLATEAU_F) {
                    if (k1 != B_OCEAN && k1 != B_DEEP_OCEAN && k1 != B_RIVER && k1 != B_SWAMPLAND)
                        *o = !oceanic(g, n) && !oceanic(g, e) && !oceanic(g, wv) && !oceanic(g, s) ? k1 : B_BEACH;
                    else *o = k1;
                } else {
                    if (!oceanic(g, n) && !oceanic(g, e) && !oceanic(g, wv) && !oceanic(g, s))
                        *o = is_mesa(g, n) && is_mesa(g, e) && is_mesa(g, wv) && is_mesa(g, s) ? k1 : B_DESERT;
                    else *o = k1;
                }
            } else *o = shore_replace(g, p, j1, i1, w, k1, B_STONE_BEACH);
        }
}

static void river_mix(const gl_world *g, gl_arena *a, const gl_layer *L, int x, int z, int w, int h, int32_t *out)
{
    int32_t *p = take(a, w * h), *q = take(a, w * h);
    ints(g, a, L->in0, x, z, w, h, p);
    ints(g, a, L->in1, x, z, w, h, q);
    for (int i1 = 0; i1 < w * h; ++i1) {
        if (p[i1] != B_OCEAN && p[i1] != B_DEEP_OCEAN) {
            if (q[i1] == B_RIVER) {
                if (p[i1] == B_ICE_PLAINS) out[i1] = B_FROZEN_RIVER;
                else if (p[i1] != B_MUSHROOM_ISLAND && p[i1] != B_MUSHROOM_SHORE) out[i1] = q[i1] & 255;
                else out[i1] = B_MUSHROOM_SHORE;
            } else out[i1] = p[i1];
        } else out[i1] = p[i1];
    }
}

static inline double vjit(rng *r) { return ((double)next_int(r, 1024) / 1024.0 - 0.5) * 3.6; }

static void voronoi(const gl_world *g, gl_arena *a, const gl_layer *L, int x, int z, int w, int h, int32_t *out)
{
    x -= 2;
    z -= 2;
    int i1 = x >> 2, j1 = z >> 2, k1 = (w >> 2) + 2, l1 = (h >> 2) + 2;
    int32_t *p = take(a, k1 * l1);
    ints(g, a, L->in0, i1, j1, k1, l1, p);
    int i2 = (k1 - 1) << 2, j2 = (l1 - 1) << 2;
    int32_t *q = take(a, i2 * j2);
    rng r = { 0, L->wgs };
    for (int k2 = 0; k2 < l1 - 1; ++k2) {
        int l2 = 0;
        int i3 = p[l2 + 0 + (k2 + 0) * k1];
        int j3 = p[l2 + 0 + (k2 + 1) * k1];
        for (; l2 < k1 - 1; ++l2) {
            init_chunk(&r, (int64_t)((l2 + i1) << 2), (int64_t)((k2 + j1) << 2));
            double d1 = vjit(&r);
            double d2 = vjit(&r);
            init_chunk(&r, (int64_t)((l2 + i1 + 1) << 2), (int64_t)((k2 + j1) << 2));
            double d3 = vjit(&r) + 4.0;
            double d4 = vjit(&r);
            init_chunk(&r, (int64_t)((l2 + i1) << 2), (int64_t)((k2 + j1 + 1) << 2));
            double d5 = vjit(&r);
            double d6 = vjit(&r) + 4.0;
            init_chunk(&r, (int64_t)((l2 + i1 + 1) << 2), (int64_t)((k2 + j1 + 1) << 2));
            double d7 = vjit(&r) + 4.0;
            double d8 = vjit(&r) + 4.0;
            int k3 = p[l2 + 1 + (k2 + 0) * k1] & 255;
            int l3 = p[l2 + 1 + (k2 + 1) * k1] & 255;
            for (int i4 = 0; i4 < 4; ++i4) {
                int j4 = ((k2 << 2) + i4) * i2 + (l2 << 2);
                for (int k4 = 0; k4 < 4; ++k4) {
                    double di = (double)i4, dk = (double)k4;
                    double d9 = (di - d2) * (di - d2) + (dk - d1) * (dk - d1);
                    double d10 = (di - d4) * (di - d4) + (dk - d3) * (dk - d3);
                    double d11 = (di - d6) * (di - d6) + (dk - d5) * (dk - d5);
                    double d12 = (di - d8) * (di - d8) + (dk - d7) * (dk - d7);
                    if (d9 < d10 && d9 < d11 && d9 < d12) q[j4++] = i3;
                    else if (d10 < d9 && d10 < d11 && d10 < d12) q[j4++] = k3;
                    else if (d11 < d9 && d11 < d10 && d11 < d12) q[j4++] = j3;
                    else q[j4++] = l3;
                }
            }
            i3 = k3;
            j3 = l3;
        }
    }
    for (int l2 = 0; l2 < h; ++l2) memcpy(out + l2 * w, q + (l2 + (z & 3)) * i2 + (x & 3), sizeof(int32_t) * (size_t)w);
}

static void ints(const gl_world *g, gl_arena *a, int li, int x, int z, int w, int h, int32_t *out)
{
    const gl_layer *L = &g->l[li];
    switch (L->kind) {
    case GL_ISLAND: island(L, x, z, w, h, out); break;
    case GL_FUZZYZOOM: zoom(g, a, L, 1, x, z, w, h, out); break;
    case GL_ZOOM: zoom(g, a, L, 0, x, z, w, h, out); break;
    case GL_ADDISLAND: add_island(g, a, L, x, z, w, h, out); break;
    case GL_REMOVEOCEAN: remove_ocean(g, a, L, x, z, w, h, out); break;
    case GL_ADDSNOW: add_snow(g, a, L, x, z, w, h, out); break;
    case GL_EDGE: edge(g, a, L, x, z, w, h, out); break;
    case GL_ADDMUSHROOM: add_mushroom(g, a, L, x, z, w, h, out); break;
    case GL_DEEPOCEAN: deep_ocean(g, a, L, x, z, w, h, out); break;
    case GL_RIVERINIT: river_init(g, a, L, x, z, w, h, out); break;
    case GL_BIOME: biome(g, a, L, x, z, w, h, out); break;
    case GL_BIOMEEDGE: biome_edge(g, a, L, x, z, w, h, out); break;
    case GL_HILLS: hills(g, a, L, x, z, w, h, out); break;
    case GL_RIVER: river(g, a, L, x, z, w, h, out); break;
    case GL_SMOOTH: smooth(g, a, L, x, z, w, h, out); break;
    case GL_RAREBIOME: rare_biome(g, a, L, x, z, w, h, out); break;
    case GL_SHORE: shore(g, a, L, x, z, w, h, out); break;
    case GL_RIVERMIX: river_mix(g, a, L, x, z, w, h, out); break;
    case GL_VORONOI: voronoi(g, a, L, x, z, w, h, out); break;
    }
}

void gl_ints(const gl_world *g, gl_arena *a, int layer, int x, int z, int w, int h, int32_t *out)
{
    a->used = 0;
    ints(g, a, layer, x, z, w, h, out);
}

static uint16_t biome_id(const gl_world *g, int32_t v)
{
    int p = get_biome(g, v);
    return p < 0 ? 0xFFFF : g->bid[p];
}

void gl_biomes10(const gl_world *g, gl_arena *a, int cx, int cz, uint16_t out[100])
{
    int32_t v[100];
    gl_ints(g, a, g->root10, cx * 4 - 2, cz * 4 - 2, 10, 10, v);
    for (int i = 0; i < 100; i++) out[i] = biome_id(g, v[i]);
}

void gl_biomes16(const gl_world *g, gl_arena *a, int cx, int cz, uint16_t out[256])
{
    int32_t v[256];
    gl_ints(g, a, g->root16, cx * 16, cz * 16, 16, 16, v);
    for (int i = 0; i < 256; i++) out[i] = biome_id(g, v[i]);
}

/* ---- loading ---- */

int gl_load(gl_world *g, const capfile *f, const char *path)
{
    memset(g, 0, sizeof *g);
    uint32_t len;
    const uint8_t *s = cap_section(f, "BIOX", &len);
    const uint8_t *t = cap_section(f, "LAYR", &len);
    if (!s || !t) return 0;
    if (rd32(s) != 256) { fprintf(stderr, "%s: BIOX holds %d biomes, not 256\n", path, rd32(s)); return -1; }
    for (int i = 0; i < 256; i++) {
        const uint8_t *b = s + 4 + 8 * i;
        g->present[i] = b[0];
        g->cls[i] = b[1];
        g->temp[i] = b[2];
        g->flags[i] = b[3];
        g->bid[i] = b[0] ? (uint16_t)rd32(b + 4) : 0xFFFF;
    }
    const uint8_t *end = t + len;
    g->n = rd32(t);
    g->root10 = rd32(t + 4);
    g->root16 = rd32(t + 8);
    t += 12;
    g->l = calloc((size_t)g->n, sizeof(gl_layer));
    for (int i = 0; i < g->n; i++) {
        gl_layer *L = &g->l[i];
        if (t + 26 > end) goto short_section;
        L->kind = t[0];
        L->mode = t[1];
        L->base = rd64(t + 2);
        L->wgs = rd64(t + 10);
        L->in0 = rd32(t + 18);
        L->in1 = rd32(t + 22);
        t += 26;
        if (L->kind >= GL_KINDS || L->in0 >= i || L->in1 >= i) {
            fprintf(stderr, "%s: LAYR layer %d: kind %d, inputs %d %d\n", path, i, L->kind, L->in0, L->in1);
            return -1;
        }
        if (L->kind == GL_BIOME)
            for (int ty = 0; ty < 4; ty++) {
                if (t + 5 > end) goto short_section;
                L->modded[ty] = t[0];
                L->n[ty] = rd32(t + 1);
                t += 5;
                if (t + 8 * L->n[ty] > end) goto short_section;
                L->list[ty] = malloc(sizeof(gl_entry) * (size_t)(L->n[ty] ? L->n[ty] : 1));
                for (int k = 0; k < L->n[ty]; k++) {
                    L->list[ty][k].id = rd32(t);
                    L->list[ty][k].weight = rd32(t + 4);
                    L->total[ty] += L->list[ty][k].weight;
                    t += 8;
                }
            }
    }
    g->have = 1;
    return 0;
short_section:
    fprintf(stderr, "%s: LAYR is short\n", path);
    return -1;
}

void gl_free(gl_world *g)
{
    for (int i = 0; i < g->n; i++)
        for (int ty = 0; ty < 4; ty++) free(g->l[i].list[ty]);
    free(g->l);
    memset(g, 0, sizeof *g);
}

void gl_slots10(const gl_world *g, gl_arena *a, int cx, int cz, uint16_t out[100])
{
    int32_t v[100];
    gl_ints(g, a, g->root10, cx * 4 - 2, cz * 4 - 2, 10, 10, v);
    for (int i = 0; i < 100; i++) {
        int p = get_biome(g, v[i]);
        out[i] = p < 0 ? 0xFFFF : (uint16_t)p;
    }
}

void gl_slots16(const gl_world *g, gl_arena *a, int cx, int cz, uint16_t out[256])
{
    int32_t v[256];
    gl_ints(g, a, g->root16, cx * 16, cz * 16, 16, 16, v);
    for (int i = 0; i < 256; i++) {
        int p = get_biome(g, v[i]);
        out[i] = p < 0 ? 0xFFFF : (uint16_t)p;
    }
}
