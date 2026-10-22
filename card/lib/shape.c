/* shape.c: 1.7.10's terrain shape in C, operation for operation as the decompiled Java. See shape.md. */
#include "shape.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Rounds to float through memory: on the card's x87 a float operation runs at the precision control's 53 bits
 * and stays there in a register; storing it rounds it to 24, which for one +, -, * or / of floats gives the
 * IEEE single result (53 >= 2 * 24 + 2). Every float operation below is wrapped in it. */
static inline float rf(float v)
{
    volatile float t = v;
    return t;
}

/* NoiseGeneratorImproved's gradient tables: field_152381_e, _f, _g (3D) and field_152384_h, _i (2D). */
static const double GX[16] = { 1, -1, 1, -1, 1, -1, 1, -1, 0, 0, 0, 0, 1, 0, -1, 0 };
static const double GY[16] = { 1, 1, -1, -1, 0, 0, 0, 0, 1, -1, 1, -1, 1, -1, 1, -1 };
static const double GZ[16] = { 0, 0, 0, 0, 1, 1, -1, -1, 1, 1, -1, -1, 0, 1, 0, -1 };
static const double HX[16] = { 1, -1, 1, -1, 1, -1, 1, -1, 0, 0, 0, 0, 1, 0, -1, 0 };
static const double HZ[16] = { 0, 0, 0, 0, 1, 1, -1, -1, 1, 1, -1, -1, 0, 1, 0, -1 };

static inline double lerp(double t, double a, double b) { return a + t * (b - a); }
static inline double grad(int h, double x, double y, double z) { h &= 15; return GX[h] * x + GY[h] * y + GZ[h] * z; }
static inline double grad2(int h, double x, double z) { h &= 15; return HX[h] * x + HZ[h] * z; }
static inline double fade(double d) { return d * d * d * (d * (d * 6.0 - 15.0) + 10.0); }

/* (int) d, minus one when that rounded up: Java's cast and the floor correction of populateNoiseArray. */
static inline int ifloor(double d)
{
    int i = (int)d;
    return d < (double)i ? i - 1 : i;
}

/* MathHelper.floor_double_long. */
static inline int64_t lfloor(double d)
{
    int64_t i = (int64_t)d;
    return d < (double)i ? i - 1 : i;
}

/* NoiseGeneratorImproved.populateNoiseArray. */
static void populate(const octave *g, double *out, double xo, double yo, double zo, int xs, int ys, int zs,
                     double xsc, double ysc, double zsc, double ns)
{
    const int32_t *p = g->p;
    if (ys == 1) {
        int k5 = 0;
        double d23 = 1.0 / ns;
        for (int j1 = 0; j1 < xs; ++j1) {
            double d9 = xo + (double)j1 * xsc + g->x;
            int i6 = ifloor(d9);
            int k1 = i6 & 255;
            d9 -= (double)i6;
            double d11 = fade(d9);
            for (int l1 = 0; l1 < zs; ++l1) {
                double d12 = zo + (double)l1 * zsc + g->z;
                int i2 = ifloor(d12);
                int j2 = i2 & 255;
                d12 -= (double)i2;
                double d13 = fade(d12);
                int l = p[k1] + 0;
                int i4 = p[l] + j2;
                int j4 = p[k1 + 1] + 0;
                int i1 = p[j4] + j2;
                double d21 = lerp(d11, grad2(p[i4], d9, d12), grad(p[i1], d9 - 1.0, 0.0, d12));
                double d22 = lerp(d11, grad(p[i4 + 1], d9, 0.0, d12 - 1.0), grad(p[i1 + 1], d9 - 1.0, 0.0, d12 - 1.0));
                double d24 = lerp(d13, d21, d22);
                out[k5++] += d24 * d23;
            }
        }
        return;
    }

    int l = 0;
    double d7 = 1.0 / ns;
    int i1 = -1;
    double d8 = 0, d9 = 0, d10 = 0, d11 = 0;
    for (int l1 = 0; l1 < xs; ++l1) {
        double d12 = xo + (double)l1 * xsc + g->x;
        int i2 = ifloor(d12);
        int j2 = i2 & 255;
        d12 -= (double)i2;
        double d13 = fade(d12);
        for (int k2 = 0; k2 < zs; ++k2) {
            double d14 = zo + (double)k2 * zsc + g->z;
            int l2 = ifloor(d14);
            int i3 = l2 & 255;
            d14 -= (double)l2;
            double d15 = fade(d14);
            for (int j3 = 0; j3 < ys; ++j3) {
                double d16 = yo + (double)j3 * ysc + g->y;
                int k3 = ifloor(d16);
                int l3 = k3 & 255;
                d16 -= (double)k3;
                double d17 = fade(d16);
                /* Recomputed only at the first height of each vertical cell, with that height's d16: the
                 * values are then reused for the rest of the cell, as 1.7.10 does. */
                if (j3 == 0 || l3 != i1) {
                    i1 = l3;
                    int k4 = p[j2] + l3;
                    int l4 = p[k4] + i3;
                    int i5 = p[k4 + 1] + i3;
                    int j5 = p[j2 + 1] + l3;
                    int k5 = p[j5] + i3;
                    int l5 = p[j5 + 1] + i3;
                    d8 = lerp(d13, grad(p[l4], d12, d16, d14), grad(p[k5], d12 - 1.0, d16, d14));
                    d9 = lerp(d13, grad(p[i5], d12, d16 - 1.0, d14), grad(p[l5], d12 - 1.0, d16 - 1.0, d14));
                    d10 = lerp(d13, grad(p[l4 + 1], d12, d16, d14 - 1.0), grad(p[k5 + 1], d12 - 1.0, d16, d14 - 1.0));
                    d11 = lerp(d13, grad(p[i5 + 1], d12, d16 - 1.0, d14 - 1.0),
                               grad(p[l5 + 1], d12 - 1.0, d16 - 1.0, d14 - 1.0));
                }
                double d18 = lerp(d17, d8, d9);
                double d19 = lerp(d17, d10, d11);
                double d20 = lerp(d15, d18, d19);
                out[l++] += d20 * d7;
            }
        }
    }
}

void noise_octaves(const octaves *g, double *out, int x, int y, int z, int xs, int ys, int zs, double xsc,
                   double ysc, double zsc)
{
    memset(out, 0, sizeof(double) * (size_t)(xs * ys * zs));
    double d6 = 1.0;
    for (int l1 = 0; l1 < g->n; ++l1) {
        double d3 = (double)x * d6 * xsc;
        double d4 = (double)y * d6 * ysc;
        double d5 = (double)z * d6 * zsc;
        int64_t i2 = lfloor(d3);
        int64_t j2 = lfloor(d5);
        d3 -= (double)i2;
        d5 -= (double)j2;
        i2 %= 16777216LL;
        j2 %= 16777216LL;
        d3 += (double)i2;
        d5 += (double)j2;
        populate(&g->o[l1], out, d3, d4, d5, xs, ys, zs, xsc * d6, ysc * d6, zsc * d6, d6);
        d6 /= 2.0;
    }
}

void shape_octaves(const world *w, int cx, int cz, double o0[25], double o1[825], double o2[825], double o3[825])
{
    int x = cx * 4, z = cz * 4;
    /* generateNoiseOctaves(arr, x, z, 5, 5, 200, 200, 0.5) forwards to (arr, x, 10, z, 5, 1, 5, 200, 1.0, 200). */
    noise_octaves(&w->gen[0], o0, x, 10, z, 5, 1, 5, 200.0, 1.0, 200.0);
    noise_octaves(&w->gen[1], o1, x, 0, z, 5, 33, 5, 8.555150000000001, 4.277575000000001, 8.555150000000001);
    noise_octaves(&w->gen[2], o2, x, 0, z, 5, 33, 5, 684.412, 684.412, 684.412);
    noise_octaves(&w->gen[3], o3, x, 0, z, 5, 33, 5, 684.412, 684.412, 684.412);
}

void shape_combine(const world *w, const uint16_t bi10[100], const double o0[25], const double o1[825],
                   const double o2[825], const double o3[825], double field[825])
{
    int l = 0, i1 = 0;
    for (int j1 = 0; j1 < 5; ++j1) {
        for (int k1 = 0; k1 < 5; ++k1) {
            float f = 0.0f, f1 = 0.0f, f2 = 0.0f;
            int centre = bi10[j1 + 2 + (k1 + 2) * 10];
            for (int l1 = -2; l1 <= 2; ++l1) {
                for (int i2 = -2; i2 <= 2; ++i2) {
                    int b = bi10[j1 + l1 + 2 + (k1 + i2 + 2) * 10];
                    float f3 = w->root[b], f4 = w->var[b];
                    if (w->amplified && f3 > 0.0f) {
                        f3 = rf(1.0f + rf(f3 * 2.0f));
                        f4 = rf(1.0f + rf(f4 * 4.0f));
                    }
                    float f5 = rf(w->parabolic[l1 + 2 + (i2 + 2) * 5] / rf(f3 + 2.0f));
                    if (w->root[b] > w->root[centre]) f5 = rf(f5 / 2.0f);
                    f = rf(f + rf(f4 * f5));
                    f1 = rf(f1 + rf(f3 * f5));
                    f2 = rf(f2 + f5);
                }
            }
            f = rf(f / f2);
            f1 = rf(f1 / f2);
            f = rf(rf(f * 0.9f) + 0.1f);
            f1 = rf(rf(rf(f1 * 4.0f) - 1.0f) / 8.0f);
            double d12 = o0[i1] / 8000.0;
            if (d12 < 0.0) d12 = -d12 * 0.3;
            d12 = d12 * 3.0 - 2.0;
            if (d12 < 0.0) {
                d12 /= 2.0;
                if (d12 < -1.0) d12 = -1.0;
                d12 /= 1.4;
                d12 /= 2.0;
            } else {
                if (d12 > 1.0) d12 = 1.0;
                d12 /= 8.0;
            }
            ++i1;
            double d13 = (double)f1;
            double d14 = (double)f;
            d13 += d12 * 0.2;
            d13 = d13 * 8.5 / 8.0;
            double d5 = 8.5 + d13 * 4.0;
            for (int j2 = 0; j2 < 33; ++j2) {
                double d6 = ((double)j2 - d5) * 12.0 * 128.0 / 256.0 / d14;
                if (d6 < 0.0) d6 *= 4.0;
                double d7 = o2[l] / 512.0;
                double d8 = o3[l] / 512.0;
                double d9 = (o1[l] / 10.0 + 1.0) / 2.0;
                /* MathHelper.denormalizeClamp(d7, d8, d9) */
                double clamp = d9 < 0.0 ? d7 : (d9 > 1.0 ? d8 : d7 + (d8 - d7) * d9);
                double d10 = clamp - d6;
                if (j2 > 29) {
                    double d11 = (double)rf(rf((float)(j2 - 29)) / 3.0f);
                    d10 = d10 * (1.0 - d11) + -10.0 * d11;
                }
                field[l++] = d10;
            }
        }
    }
}

void shape_interp(const double field[825], uint16_t blocks[65536])
{
    const int sea = 63;
    for (int k = 0; k < 4; ++k) {
        int l = k * 5, i1 = (k + 1) * 5;
        for (int j1 = 0; j1 < 4; ++j1) {
            int k1 = (l + j1) * 33, l1 = (l + j1 + 1) * 33, i2 = (i1 + j1) * 33, j2 = (i1 + j1 + 1) * 33;
            for (int k2 = 0; k2 < 32; ++k2) {
                double d0 = 0.125;
                double d1 = field[k1 + k2], d2 = field[l1 + k2], d3 = field[i2 + k2], d4 = field[j2 + k2];
                double d5 = (field[k1 + k2 + 1] - d1) * d0;
                double d6 = (field[l1 + k2 + 1] - d2) * d0;
                double d7 = (field[i2 + k2 + 1] - d3) * d0;
                double d8 = (field[j2 + k2 + 1] - d4) * d0;
                for (int l2 = 0; l2 < 8; ++l2) {
                    double d9 = 0.25;
                    double d10 = d1, d11 = d2;
                    double d12 = (d3 - d1) * d9, d13 = (d4 - d2) * d9;
                    for (int i3 = 0; i3 < 4; ++i3) {
                        int j3 = ((i3 + k * 4) << 12 | (0 + j1 * 4) << 8 | (k2 * 8 + l2)) - 256;
                        double d14 = 0.25;
                        double d16 = (d11 - d10) * d14;
                        double d15 = d10 - d16;
                        for (int k3 = 0; k3 < 4; ++k3) {
                            j3 += 256;
                            if ((d15 += d16) > 0.0) blocks[j3] = BLOCK_STONE;
                            else if (k2 * 8 + l2 < sea) blocks[j3] = BLOCK_WATER;
                            else blocks[j3] = BLOCK_NULL;
                        }
                        d10 += d12;
                        d11 += d13;
                    }
                    d1 += d5;
                    d2 += d6;
                    d3 += d7;
                    d4 += d8;
                }
            }
        }
    }
}

void shape_chunk(const world *w, int cx, int cz, const uint16_t bi10[100], uint16_t blocks[65536])
{
    double o0[25], o1[825], o2[825], o3[825], field[825];
    shape_octaves(w, cx, cz, o0, o1, o2, o3);
    shape_combine(w, bi10, o0, o1, o2, o3, field);
    shape_interp(field, blocks);
}

size_t shape_rle(const uint16_t blocks[65536], uint8_t *out)
{
    size_t n = 0;
    for (int c = 0; c < 256; c++) {
        const uint16_t *col = blocks + (c << 8);
        int y = 0;
        while (y < 256) {
            uint16_t t = col[y];
            int run = 1;
            while (y + run < 256 && col[y + run] == t) run++;
            out[n++] = t == BLOCK_STONE ? 1 : t == BLOCK_WATER ? 2 : 0;
            out[n++] = (uint8_t)(run - 1);
            y += run;
        }
    }
    return n;
}

int world_load(world *w, const capfile *f, const char *path)
{
    uint32_t n;
    const uint8_t *s = cap_need(f, path, "SEED", 11);
    int name_len = rd16(s + 9);
    w->amplified = name_len == 9 && memcmp(s + 11, "amplified", 9) == 0;

    s = cap_need(f, path, "GENS", 4);
    if (rd32(s) != 4) { fprintf(stderr, "%s: %d generators, want 4\n", path, rd32(s)); return -1; }
    s += 4;
    for (int g = 0; g < 4; g++) {
        w->gen[g].n = rd32(s);
        s += 4;
        w->gen[g].o = calloc((size_t)w->gen[g].n, sizeof(octave));
        for (int o = 0; o < w->gen[g].n; o++) {
            octave *oc = &w->gen[g].o[o];
            for (int i = 0; i < 512; i++) oc->p[i] = rd32(s + 4 * i);
            s += 2048;
            oc->x = rdf64(s);
            oc->y = rdf64(s + 8);
            oc->z = rdf64(s + 16);
            s += 24;
        }
    }

    s = cap_need(f, path, "PARB", 100);
    for (int i = 0; i < 25; i++) w->parabolic[i] = rdf32(s + 4 * i);

    s = cap_section(f, "BIOM", &n);
    if (!s || rd32(s) != 256) { fprintf(stderr, "%s: BIOM missing or not 256 biomes\n", path); return -1; }
    s += 4;
    for (int b = 0; b < 256; b++) {
        w->root[b] = rdf32(s + 1);
        w->var[b] = rdf32(s + 5);
        s += 1 + 4 + 4 + 2 + 2;
        s += 2 + rd16(s);
    }
    return 0;
}
