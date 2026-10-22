/* surface.c: replaceBlocksForBiome, ported from 1.7.10 + Forge 10.13.4.1614. See surface.md. */
#include "surface.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define NULLB 0xFFFF

/* ---- java.util.Random (JDK 8) ---- */

#define JR_MULT 0x5DEECE66DULL
#define JR_MASK ((1ULL << 48) - 1)

void jr_seed(jrand *r, int64_t seed) { r->s = ((uint64_t)seed ^ JR_MULT) & JR_MASK; }

int jr_next(jrand *r, int bits)
{
    r->s = (r->s * JR_MULT + 0xBULL) & JR_MASK;
    return (int32_t)(uint32_t)(r->s >> (48 - bits));
}

int jr_int(jrand *r, int bound)
{
    if ((bound & -bound) == bound) return (int)(((int64_t)bound * (int64_t)jr_next(r, 31)) >> 31);
    int bits, val;
    do {
        bits = jr_next(r, 31);
        val = bits % bound;
    } while ((int32_t)((uint32_t)bits - (uint32_t)val + (uint32_t)(bound - 1)) < 0);
    return val;
}

double jr_double(jrand *r)
{
    int64_t hi = jr_next(r, 26), lo = jr_next(r, 27);
    return (double)((hi << 27) + lo) * 0x1.0p-53;
}

int64_t jr_long(jrand *r)
{
    int64_t hi = jr_next(r, 32), lo = jr_next(r, 32);
    return (int64_t)(((uint64_t)hi << 32) + (uint64_t)lo);
}

float jr_float(jrand *r) { return (float)jr_next(r, 24) / (float)(1 << 24); }

/* ---- NoiseGeneratorSimplex.func_151606_a, NoiseGeneratorPerlin.func_151600_a ---- */

static const int GRAD3[12][3] = { { 1, 1, 0 }, { -1, 1, 0 }, { 1, -1, 0 }, { -1, -1, 0 }, { 1, 0, 1 }, { -1, 0, 1 },
                                  { 1, 0, -1 }, { -1, 0, -1 }, { 0, 1, 1 }, { 0, -1, 1 }, { 0, 1, -1 }, { 0, -1, -1 } };

static inline int sfloor(double d) { return d > 0.0 ? (int)d : (int)d - 1; }
static inline double dot(const int *g, double x, double y) { return (double)g[0] * x + (double)g[1] * y; }

static void simplex_add(const simplex *o, double *out, double x, double z, int w, int h, double xs, double zs,
                        double amp)
{
    const double sq3 = sqrt(3.0);
    const double F2 = 0.5 * (sq3 - 1.0), G2 = (3.0 - sq3) / 6.0;
    int k = 0;
    for (int l = 0; l < h; ++l) {
        double d5 = (z + (double)l) * zs + o->y;
        for (int i1 = 0; i1 < w; ++i1) {
            double d6 = (x + (double)i1) * xs + o->x;
            double d10 = (d6 + d5) * F2;
            int j1 = sfloor(d6 + d10);
            int k1 = sfloor(d5 + d10);
            double d11 = (double)(j1 + k1) * G2;
            double d12 = (double)j1 - d11;
            double d13 = (double)k1 - d11;
            double d14 = d6 - d12;
            double d15 = d5 - d13;
            int b0, b1;
            if (d14 > d15) { b0 = 1; b1 = 0; }
            else { b0 = 0; b1 = 1; }
            double d16 = d14 - (double)b0 + G2;
            double d17 = d15 - (double)b1 + G2;
            double d18 = d14 - 1.0 + 2.0 * G2;
            double d19 = d15 - 1.0 + 2.0 * G2;
            int l1 = j1 & 255, i2 = k1 & 255;
            int j2 = o->p[l1 + o->p[i2]] % 12;
            int k2 = o->p[l1 + b0 + o->p[i2 + b1]] % 12;
            int l2 = o->p[l1 + 1 + o->p[i2 + 1]] % 12;
            double d20 = 0.5 - d14 * d14 - d15 * d15, d7;
            if (d20 < 0.0) d7 = 0.0;
            else {
                d20 *= d20;
                d7 = d20 * d20 * dot(GRAD3[j2], d14, d15);
            }
            double d21 = 0.5 - d16 * d16 - d17 * d17, d8;
            if (d21 < 0.0) d8 = 0.0;
            else {
                d21 *= d21;
                d8 = d21 * d21 * dot(GRAD3[k2], d16, d17);
            }
            double d22 = 0.5 - d18 * d18 - d19 * d19, d9;
            if (d22 < 0.0) d9 = 0.0;
            else {
                d22 *= d22;
                d9 = d22 * d22 * dot(GRAD3[l2], d18, d19);
            }
            out[k] = out[k] + 70.0 * (d7 + d8 + d9) * amp;
            k++;
        }
    }
}

/* func_151599_a(out, x, z, w, h, xs, zs, 1.0): func_151600_a with p12 = 1.0, p14 = 0.5 */
static void perlin_fill(const perlin *g, double *out, double x, double z, int w, int h, double xs, double zs)
{
    memset(out, 0, sizeof(double) * (size_t)(w * h));
    double d7 = 1.0, d6 = 1.0;
    for (int l = 0; l < g->n; ++l) {
        simplex_add(&g->o[l], out, x, z, w, h, xs * d6 * d7, zs * d6 * d7, 0.55 / d7);
        d6 *= 1.0;
        d7 *= 0.5;
    }
}

/* ---- genBiomeTerrain ---- */

typedef struct {
    uint16_t top, filler;
    uint8_t meta, top_air;
    float temp;
} eff;

static inline int is_air(const surf_world *s, uint16_t id) { return id == s->air; }

static void biome_terrain(const surf_world *s, const eff *b, jrand *r, uint16_t *blocks, uint8_t *metas, int wx,
                          int wz, double noise)
{
    uint16_t block = b->top, block1 = b->filler;
    uint8_t b0 = b->meta;
    int k = -1;
    int l = (int)(noise / 3.0 + 3.0 + jr_double(r) * 0.25);
    int i1 = wx & 15, j1 = wz & 15;
    for (int l1 = 255; l1 >= 0; --l1) {
        int i2 = (j1 * 16 + i1) * 256 + l1; /* vanilla's index: x and z swapped against the shape's */
        if (l1 <= 0 + jr_int(r, 5)) {
            blocks[i2] = s->bedrock;
            continue;
        }
        uint16_t block2 = blocks[i2];
        if (block2 != NULLB && !is_air(s, block2)) {
            if (block2 != s->stone_id) continue;
            if (k == -1) {
                if (l <= 0) {
                    block = NULLB;
                    b0 = 0;
                    block1 = s->stone_id;
                } else if (l1 >= 59 && l1 <= 64) {
                    block = b->top;
                    b0 = b->meta;
                    block1 = b->filler;
                }
                if (l1 < 63 && (block == NULLB || (block == b->top ? b->top_air : is_air(s, block)))) {
                    block = b->temp < 0.15f ? s->ice : s->water; /* getFloatTemperature is the plain temperature below 65 */
                    b0 = 0;
                }
                k = l;
                if (l1 >= 62) {
                    blocks[i2] = block;
                    metas[i2] = b0;
                } else if (l1 < 56 - l) {
                    block = NULLB;
                    block1 = s->stone_id;
                    blocks[i2] = s->gravel;
                } else blocks[i2] = block1;
            } else if (k > 0) {
                --k;
                blocks[i2] = block1;
                if (k == 0 && block1 == s->sand) {
                    int extra = l1 - 63;
                    k = jr_int(r, 4) + (extra > 0 ? extra : 0);
                    block1 = s->sandstone;
                }
            }
        } else k = -1;
    }
}

int surf_chunk(const surf_world *s, int cx, int cz, const uint16_t slots16[256], uint16_t *blocks, uint8_t *metas,
               surf_state *st)
{
    /* the slots whose genTerrainBlocks runs, and the objects whose fields that call rewrites */
    for (int i = 0; i < 256; i++) {
        int sl = slots16[i];
        if (sl == 0xFFFF || !s->b[sl].present) return -1;
        const sbiome *b = &s->b[sl];
        if (b->kind == SK_MUTATED) b = &s->b[b->base];
        if (b->kind == SK_UNPORTED || b->kind == SK_MUTATED) return -1;
    }

    jrand r;
    jr_seed(&r, (int64_t)((uint64_t)(int64_t)cx * 341873128712ULL + (uint64_t)(int64_t)cz * 132897987541ULL));
    double noise[256];
    perlin_fill(&s->stone, noise, (double)(cx * 16), (double)(cz * 16), 16, 16, 0.03125 * 2.0, 0.03125 * 2.0);

    /* the fields of the objects the overrides rewrite: slot -> (top, filler, meta) as they stand now */
    surf_state now[256];
    int nst = 0;
    int index[256];
    memset(index, -1, sizeof index);

    for (int k = 0; k < 16; ++k)
        for (int l = 0; l < 16; ++l) {
            int sl = slots16[l + k * 16];
            const sbiome *call = &s->b[sl];
            int obj = sl; /* the object genTerrainBlocks and genBiomeTerrain run on */
            if (call->kind == SK_MUTATED) {
                obj = call->base;
                call = &s->b[obj];
            }
            double d = noise[l + k * 16];
            eff e = { call->top, call->filler, call->meta, call->top_air, call->temp };
            int rewrites = 0;
            if (call->kind == SK_HILLS) {
                e.top = s->grass;
                e.meta = 0;
                e.filler = s->dirt;
                if ((d < -1.0 || d > 2.0) && (call->hills & 1)) {
                    e.top = s->gravel;
                    e.filler = s->gravel;
                } else if (d > 1.0 && (call->hills & 2)) {
                    e.top = s->stone_id;
                    e.filler = s->stone_id;
                }
                rewrites = 1;
            } else if (call->kind == SK_SAVANNA_M) {
                e.top = s->grass;
                e.meta = 0;
                e.filler = s->dirt;
                if (d > 1.75) {
                    e.top = s->stone_id;
                    e.filler = s->stone_id;
                } else if (d > -0.5) {
                    e.top = s->dirt;
                    e.meta = 1;
                }
                rewrites = 1;
            } else if (call->kind == SK_TAIGA && (call->taiga == 1 || call->taiga == 2)) {
                e.top = s->grass;
                e.meta = 0;
                e.filler = s->dirt;
                if (d > 1.75) {
                    e.top = s->dirt;
                    e.meta = 1;
                } else if (d > -0.95) {
                    e.top = s->dirt;
                    e.meta = 2;
                }
                rewrites = 1;
            }
            if (rewrites) {
                e.top_air = is_air(s, e.top);
                int owner = s->b[obj].canon; /* two slots can hold one object: its fields are one */
                if (index[owner] < 0) index[owner] = nst++;
                now[index[owner]] = (surf_state){ (uint8_t)owner, e.meta, e.top, e.filler };
            }
            biome_terrain(s, &e, &r, blocks, metas, cx * 16 + k, cz * 16 + l, d);
        }
    memcpy(st, now, sizeof(surf_state) * (size_t)nst);
    return nst;
}

/* ---- loading ---- */

int surf_load(surf_world *s, const capfile *f, const char *path)
{
    memset(s, 0, sizeof *s);
    uint32_t nb, nn, nk;
    const uint8_t *b = cap_section(f, "SURB", &nb), *n = cap_section(f, "SNOI", &nn), *k = cap_section(f, "BLKS", &nk);
    if (!b || !n || !k) return 0;
    if (nk < 22 || rd32(b) != 256 || nb < 4 + 256 * 16) { fprintf(stderr, "%s: SURB or BLKS is short\n", path); return -1; }
    s->air = rd16(k);
    s->stone_id = rd16(k + 2);
    s->bedrock = rd16(k + 4);
    s->gravel = rd16(k + 6);
    s->sand = rd16(k + 8);
    s->sandstone = rd16(k + 10);
    s->water = rd16(k + 12);
    s->ice = rd16(k + 14);
    s->grass = rd16(k + 16);
    s->dirt = rd16(k + 18);
    s->lava = rd16(k + 20);
    for (int i = 0; i < 256; i++) {
        const uint8_t *e = b + 4 + 16 * i;
        sbiome *x = &s->b[i];
        x->present = e[0];
        x->kind = e[1];
        x->base = e[2];
        x->meta = e[3];
        x->hills = e[4];
        x->taiga = e[5];
        x->top_air = e[6] & 1;
        x->filler_air = (e[6] >> 1) & 1;
        x->canon = e[7];
        x->top = rd16(e + 8);
        x->filler = rd16(e + 10);
        x->temp = rdf32(e + 12);
    }
    s->stone.n = rd32(n);
    if (s->stone.n < 1 || s->stone.n > 8 || nn < 4 + (uint32_t)s->stone.n * (2048 + 24)) {
        fprintf(stderr, "%s: SNOI holds %d octaves\n", path, s->stone.n);
        return -1;
    }
    const uint8_t *p = n + 4;
    for (int o = 0; o < s->stone.n; o++) {
        for (int i = 0; i < 512; i++) s->stone.o[o].p[i] = rd32(p + 4 * i);
        s->stone.o[o].x = rdf64(p + 2048);
        s->stone.o[o].y = rdf64(p + 2056);
        s->stone.o[o].z = rdf64(p + 2064);
        p += 2048 + 24;
    }
    s->have = 1;
    return 0;
}
