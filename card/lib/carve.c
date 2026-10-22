/* carve.c: MapGenCaves and MapGenRavine of 1.7.10 + Forge 10.13.4.1614 (digBlock, isTopBlock). See carve.md. */
#include "carve.h"

#include <stdio.h>
#include <string.h>

#define NULLB 0xFFFF
#define PI_F ((float)3.14159265358979323846)

/* One float operation's IEEE single result on x87 at 53 bits (as lib/shape.c's rf). */
static inline float rf(float v)
{
    volatile float t = v;
    return t;
}

static inline float msin(const carve_world *c, float f) { return c->sin_table[(int)rf(f * 10430.378f) & 65535]; }
static inline float mcos(const carve_world *c, float f)
{
    return c->sin_table[(int)rf(rf(f * 10430.378f) + 16384.0f) & 65535];
}

static inline int floor_d(double d)
{
    int i = (int)d;
    return d < (double)i ? i - 1 : i;
}

static inline int64_t mul64(int64_t a, int64_t b) { return (int64_t)((uint64_t)a * (uint64_t)b); }

/* ---- the shared carving parts ---- */

/* The ocean test over the box: water or flowing water anywhere on its shell stops the carve. */
static int ocean_in_box(const carve_world *c, const uint16_t *blocks, int i4, int l1, int j4, int i2, int k4, int j2)
{
    for (int k2 = i4; k2 < l1; ++k2)
        for (int l2 = k4; l2 < j2; ++l2)
            for (int i3 = i2 + 1; i3 >= j4 - 1; --i3) {
                int j3 = (k2 * 16 + l2) * 256 + i3;
                if (i3 >= 0 && i3 < 256) {
                    uint16_t b = blocks[j3];
                    if (b == c->flowing_water || b == c->water) return 1;
                    if (i3 != j4 - 1 && k2 != i4 && k2 != l1 - 1 && l2 != k4 && l2 != j2 - 1) i3 = j4;
                }
            }
    return 0;
}

static inline void dig(const carve_world *c, const carve_cols *cols, uint16_t *blocks, int index, int x, int y, int z,
                       int found_top, uint16_t lava)
{
    uint16_t top = cols->top[x + z * 16], filler = cols->filler[x + z * 16];
    uint16_t block = blocks[index];
    if (block == c->stone || block == filler || block == top) {
        if (y < 10) blocks[index] = lava;
        else {
            blocks[index] = NULLB;
            if (found_top && blocks[index - 1] == filler) blocks[index - 1] = top;
        }
    }
}

/* ---- MapGenCaves ---- */

static void tunnel(const carve_world *c, const carve_cols *cols, int64_t seed, int cx, int cz, uint16_t *blocks,
                   double px, double py, double pz, float width, float yaw, float pitch, int step, int steps,
                   double ystretch)
{
    double d4 = (double)(cx * 16 + 8), d5 = (double)(cz * 16 + 8);
    float f3 = 0.0f, f4 = 0.0f;
    jrand random;
    jr_seed(&random, seed);
    if (steps <= 0) {
        int j1 = c->range_caves * 16 - 16;
        steps = j1 - jr_int(&random, j1 / 4);
    }
    int room = 0; /* flag2 */
    if (step == -1) {
        step = steps / 2;
        room = 1;
    }
    int k1 = jr_int(&random, steps / 2) + steps / 4;
    for (int steep = jr_int(&random, 6) == 0; step < steps; ++step) {
        double d6 = 1.5 + (double)rf(rf(msin(c, rf(rf((float)step * PI_F) / (float)steps)) * width) * 1.0f);
        double d7 = d6 * ystretch;
        float f5 = mcos(c, pitch), f6 = msin(c, pitch);
        px += (double)rf(mcos(c, yaw) * f5);
        py += (double)f6;
        pz += (double)rf(msin(c, yaw) * f5);
        pitch = steep ? rf(pitch * 0.92f) : rf(pitch * 0.7f);
        pitch = rf(pitch + rf(f4 * 0.1f));
        yaw = rf(yaw + rf(f3 * 0.1f));
        f4 = rf(f4 * 0.9f);
        f3 = rf(f3 * 0.75f);
        {
            float a = jr_float(&random), b = jr_float(&random), d = jr_float(&random);
            f4 = rf(f4 + rf(rf(rf(a - b) * d) * 2.0f));
        }
        {
            float a = jr_float(&random), b = jr_float(&random), d = jr_float(&random);
            f3 = rf(f3 + rf(rf(rf(a - b) * d) * 4.0f));
        }
        if (!room && step == k1 && width > 1.0f && steps > 0) {
            int64_t s1 = jr_long(&random);
            float w1 = rf(rf(jr_float(&random) * 0.5f) + 0.5f);
            tunnel(c, cols, s1, cx, cz, blocks, px, py, pz, w1, rf(yaw - rf(PI_F / 2.0f)), rf(pitch / 3.0f), step,
                   steps, 1.0);
            int64_t s2 = jr_long(&random);
            float w2 = rf(rf(jr_float(&random) * 0.5f) + 0.5f);
            tunnel(c, cols, s2, cx, cz, blocks, px, py, pz, w2, rf(yaw + rf(PI_F / 2.0f)), rf(pitch / 3.0f), step,
                   steps, 1.0);
            return;
        }
        if (room || jr_int(&random, 4) != 0) {
            double d8 = px - d4, d9 = pz - d5;
            double d10 = (double)(steps - step);
            double d11 = (double)rf(rf(width + 2.0f) + 16.0f);
            if (d8 * d8 + d9 * d9 - d10 * d10 > d11 * d11) return;
            if (px >= d4 - 16.0 - d6 * 2.0 && pz >= d5 - 16.0 - d6 * 2.0 && px <= d4 + 16.0 + d6 * 2.0 &&
                pz <= d5 + 16.0 + d6 * 2.0) {
                int i4 = floor_d(px - d6) - cx * 16 - 1;
                int l1 = floor_d(px + d6) - cx * 16 + 1;
                int j4 = floor_d(py - d7) - 1;
                int i2 = floor_d(py + d7) + 1;
                int k4 = floor_d(pz - d6) - cz * 16 - 1;
                int j2 = floor_d(pz + d6) - cz * 16 + 1;
                if (i4 < 0) i4 = 0;
                if (l1 > 16) l1 = 16;
                if (j4 < 1) j4 = 1;
                if (i2 > 248) i2 = 248;
                if (k4 < 0) k4 = 0;
                if (j2 > 16) j2 = 16;
                if (!ocean_in_box(c, blocks, i4, l1, j4, i2, k4, j2)) {
                    for (int k2 = i4; k2 < l1; ++k2) {
                        double d13 = ((double)(k2 + cx * 16) + 0.5 - px) / d6;
                        for (int j3 = k4; j3 < j2; ++j3) {
                            double d14 = ((double)(j3 + cz * 16) + 0.5 - pz) / d6;
                            int k3 = (k2 * 16 + j3) * 256 + i2;
                            int found_top = 0;
                            if (d13 * d13 + d14 * d14 < 1.0) {
                                for (int l3 = i2 - 1; l3 >= j4; --l3) {
                                    double d12 = ((double)l3 + 0.5 - py) / d7;
                                    if (d12 > -0.7 && d13 * d13 + d12 * d12 + d14 * d14 < 1.0) {
                                        if (blocks[k3] == cols->top[k2 + j3 * 16]) found_top = 1;
                                        dig(c, cols, blocks, k3, k2, l3, j3, found_top, c->lava);
                                    }
                                    --k3;
                                }
                            }
                        }
                    }
                    if (room) break;
                }
            }
        }
    }
}

static void cave_source(const carve_world *c, const carve_cols *cols, jrand *rand, int sx, int sz, int cx, int cz,
                        uint16_t *blocks)
{
    int a = jr_int(rand, 15) + 1;
    int b = jr_int(rand, a) + 1;
    int i1 = jr_int(rand, b);
    if (jr_int(rand, 7) != 0) i1 = 0;
    for (int j1 = 0; j1 < i1; ++j1) {
        double d0 = (double)(sx * 16 + jr_int(rand, 16));
        int h = jr_int(rand, 120) + 8;
        double d1 = (double)jr_int(rand, h);
        double d2 = (double)(sz * 16 + jr_int(rand, 16));
        int k1 = 1;
        if (jr_int(rand, 4) == 0) {
            int64_t s = jr_long(rand);
            float w = rf(1.0f + rf(jr_float(rand) * 6.0f)); /* func_151542_a: from the new Random, below */
            (void)w;
            /* func_151542_a(seed, ...): width 1 + rand.nextFloat() * 6, drawn from THIS rand before the call */
            tunnel(c, cols, s, cx, cz, blocks, d0, d1, d2, w, 0.0f, 0.0f, -1, -1, 0.5);
            k1 += jr_int(rand, 4);
        }
        for (int l1 = 0; l1 < k1; ++l1) {
            float f = rf(rf(jr_float(rand) * PI_F) * 2.0f);
            float f1 = rf(rf(rf(jr_float(rand) - 0.5f) * 2.0f) / 8.0f);
            float f2a = rf(jr_float(rand) * 2.0f);
            float f2 = rf(f2a + jr_float(rand));
            if (jr_int(rand, 10) == 0) {
                float x = jr_float(rand), y = jr_float(rand);
                f2 = rf(f2 * rf(rf(rf(x * y) * 3.0f) + 1.0f));
            }
            int64_t s = jr_long(rand);
            tunnel(c, cols, s, cx, cz, blocks, d0, d1, d2, f2, f, f1, 0, 0, 1.0);
        }
    }
}

/* ---- MapGenRavine ---- */

static void ravine(const carve_world *c, const carve_cols *cols, int64_t seed, int cx, int cz, uint16_t *blocks,
                   double px, double py, double pz, float width, float yaw, float pitch, int step, int steps,
                   double ystretch)
{
    jrand random;
    jr_seed(&random, seed);
    double d4 = (double)(cx * 16 + 8), d5 = (double)(cz * 16 + 8);
    float f3 = 0.0f, f4 = 0.0f;
    if (steps <= 0) {
        int j1 = c->range_ravines * 16 - 16;
        steps = j1 - jr_int(&random, j1 / 4);
    }
    int room = 0;
    if (step == -1) {
        step = steps / 2;
        room = 1;
    }
    float widths[256];
    float f5 = 1.0f;
    for (int k1 = 0; k1 < 256; ++k1) {
        if (k1 == 0 || jr_int(&random, 3) == 0) {
            float a = jr_float(&random), b = jr_float(&random);
            f5 = rf(1.0f + rf(rf(a * b) * 1.0f));
        }
        widths[k1] = rf(f5 * f5);
    }
    for (; step < steps; ++step) {
        double d12 = 1.5 + (double)rf(rf(msin(c, rf(rf((float)step * PI_F) / (float)steps)) * width) * 1.0f);
        double d6 = d12 * ystretch;
        d12 *= (double)jr_float(&random) * 0.25 + 0.75;
        d6 *= (double)jr_float(&random) * 0.25 + 0.75;
        float f6 = mcos(c, pitch), f7 = msin(c, pitch);
        px += (double)rf(mcos(c, yaw) * f6);
        py += (double)f7;
        pz += (double)rf(msin(c, yaw) * f6);
        pitch = rf(pitch * 0.7f);
        pitch = rf(pitch + rf(f4 * 0.05f));
        yaw = rf(yaw + rf(f3 * 0.05f));
        f4 = rf(f4 * 0.8f);
        f3 = rf(f3 * 0.5f);
        {
            float a = jr_float(&random), b = jr_float(&random), d = jr_float(&random);
            f4 = rf(f4 + rf(rf(rf(a - b) * d) * 2.0f));
        }
        {
            float a = jr_float(&random), b = jr_float(&random), d = jr_float(&random);
            f3 = rf(f3 + rf(rf(rf(a - b) * d) * 4.0f));
        }
        if (room || jr_int(&random, 4) != 0) {
            double d7 = px - d4, d8 = pz - d5;
            double d9 = (double)(steps - step);
            double d10 = (double)rf(rf(width + 2.0f) + 16.0f);
            if (d7 * d7 + d8 * d8 - d9 * d9 > d10 * d10) return;
            if (px >= d4 - 16.0 - d12 * 2.0 && pz >= d5 - 16.0 - d12 * 2.0 && px <= d4 + 16.0 + d12 * 2.0 &&
                pz <= d5 + 16.0 + d12 * 2.0) {
                int i4 = floor_d(px - d12) - cx * 16 - 1;
                int l1 = floor_d(px + d12) - cx * 16 + 1;
                int j4 = floor_d(py - d6) - 1;
                int i2 = floor_d(py + d6) + 1;
                int k4 = floor_d(pz - d12) - cz * 16 - 1;
                int j2 = floor_d(pz + d12) - cz * 16 + 1;
                if (i4 < 0) i4 = 0;
                if (l1 > 16) l1 = 16;
                if (j4 < 1) j4 = 1;
                if (i2 > 248) i2 = 248;
                if (k4 < 0) k4 = 0;
                if (j2 > 16) j2 = 16;
                if (!ocean_in_box(c, blocks, i4, l1, j4, i2, k4, j2)) {
                    for (int k2 = i4; k2 < l1; ++k2) {
                        double d13 = ((double)(k2 + cx * 16) + 0.5 - px) / d12;
                        for (int j3 = k4; j3 < j2; ++j3) {
                            double d14 = ((double)(j3 + cz * 16) + 0.5 - pz) / d12;
                            int k3 = (k2 * 16 + j3) * 256 + i2;
                            int found_top = 0;
                            if (d13 * d13 + d14 * d14 < 1.0) {
                                for (int l3 = i2 - 1; l3 >= j4; --l3) {
                                    double d11 = ((double)l3 + 0.5 - py) / d6;
                                    if ((d13 * d13 + d14 * d14) * (double)widths[l3] + d11 * d11 / 6.0 < 1.0) {
                                        if (blocks[k3] == cols->top[k2 + j3 * 16]) found_top = 1;
                                        dig(c, cols, blocks, k3, k2, l3, j3, found_top, c->flowing_lava);
                                    }
                                    --k3;
                                }
                            }
                        }
                    }
                    if (room) break;
                }
            }
        }
    }
}

static void ravine_source(const carve_world *c, const carve_cols *cols, jrand *rand, int sx, int sz, int cx, int cz,
                          uint16_t *blocks)
{
    if (jr_int(rand, 50) != 0) return;
    double d0 = (double)(sx * 16 + jr_int(rand, 16));
    int h = jr_int(rand, 40) + 8;
    double d1 = (double)(jr_int(rand, h) + 20);
    double d2 = (double)(sz * 16 + jr_int(rand, 16));
    float f = rf(rf(jr_float(rand) * PI_F) * 2.0f);
    float f1 = rf(rf(rf(jr_float(rand) - 0.5f) * 2.0f) / 8.0f);
    float f2a = rf(jr_float(rand) * 2.0f);
    float f2 = rf(rf(f2a + jr_float(rand)) * 2.0f);
    int64_t s = jr_long(rand);
    ravine(c, cols, s, cx, cz, blocks, d0, d1, d2, f2, f, f1, 0, 0, 3.0);
}

/* ---- MapGenBase.func_151539_a ---- */

typedef void (*source_fn)(const carve_world *, const carve_cols *, jrand *, int, int, int, int, uint16_t *);

static void each_source(const carve_world *c, const carve_cols *cols, int range, source_fn fn, int cx, int cz,
                        uint16_t *blocks)
{
    jrand rand;
    jr_seed(&rand, c->seed);
    int64_t l = jr_long(&rand), i1 = jr_long(&rand);
    for (int j1 = cx - range; j1 <= cx + range; ++j1)
        for (int k1 = cz - range; k1 <= cz + range; ++k1) {
            jr_seed(&rand, mul64(j1, l) ^ mul64(k1, i1) ^ c->seed);
            fn(c, cols, &rand, j1, k1, cx, cz, blocks);
        }
}

void carve_caves(const carve_world *c, const carve_cols *cols, int cx, int cz, uint16_t *blocks)
{
    each_source(c, cols, c->range_caves, cave_source, cx, cz, blocks);
}

void carve_ravines(const carve_world *c, const carve_cols *cols, int cx, int cz, uint16_t *blocks)
{
    each_source(c, cols, c->range_ravines, ravine_source, cx, cz, blocks);
}

/* ---- columns and loading ---- */

void carve_columns(const surf_world *s, const uint16_t slots16[256], const surf_state *st, int nst, carve_cols *out)
{
    for (int i = 0; i < 256; i++) {
        int sl = slots16[i];
        if (sl == 0xFFFF) {
            out->top[i] = out->filler[i] = NULLB;
            continue;
        }
        int canon = s->b[sl].canon;
        if (canon == 14 || canon == 16 || canon == 2) { /* isExceptionBiome: the objects mushroomIsland, beach, desert */
            out->top[i] = s->grass;
            out->filler[i] = s->dirt;
            continue;
        }
        uint16_t top = s->b[sl].top, filler = s->b[sl].filler;
        for (int k = 0; k < nst; k++)
            if (st[k].slot == canon) {
                top = st[k].top;
                filler = st[k].filler;
            }
        out->top[i] = top;
        out->filler[i] = filler;
    }
}

int carve_load(carve_world *c, const capfile *f, const char *path)
{
    memset(c, 0, sizeof *c);
    uint32_t nc, ns, nk, nsd;
    const uint8_t *cv = cap_section(f, "CARV", &nc), *si = cap_section(f, "SINT", &ns);
    const uint8_t *k = cap_section(f, "BLKS", &nk), *sd = cap_section(f, "SEED", &nsd);
    if (!cv || !si || !k || !sd || nk < 26) return 0;
    if (nc < 8 || ns < 65536 * 4 || nsd < 8) { fprintf(stderr, "%s: CARV, SINT or SEED is short\n", path); return -1; }
    c->seed = rd64(sd);
    c->range_caves = rd32(cv);
    c->range_ravines = rd32(cv + 4);
    for (int i = 0; i < 65536; i++) c->sin_table[i] = rdf32(si + 4 * i);
    c->stone = rd16(k + 2);
    c->water = rd16(k + 12);
    c->grass = rd16(k + 16);
    c->dirt = rd16(k + 18);
    c->lava = rd16(k + 20);
    c->flowing_water = rd16(k + 22);
    c->flowing_lava = rd16(k + 24);
    c->have = 1;
    return 0;
}
