/* vshape.c: the 3D octave noise of the terrain shape on the card's vector unit. See vshape.md. */
#include "vshape.h"

#include <immintrin.h>
#include <string.h>

typedef double v8d __attribute__((vector_size(64)));
typedef int v16i __attribute__((vector_size(64)));
typedef float v16f __attribute__((vector_size(64)));

#define A64 __attribute__((aligned(64)))

/* NoiseGeneratorImproved's 3D gradient tables (field_152381_e, _f, _g) as floats: 1, -1 and 0 convert exactly. */
static const float GXF[16] A64 = { 1, -1, 1, -1, 1, -1, 1, -1, 0, 0, 0, 0, 1, 0, -1, 0 };
static const float GYF[16] A64 = { 1, 1, -1, -1, 0, 0, 0, 0, 1, -1, 1, -1, 1, -1, 1, -1 };
static const float GZF[16] A64 = { 0, 0, 0, 0, 1, 1, -1, -1, 1, 1, -1, -1, 0, 1, 0, -1 };

static inline v8d add(v8d a, v8d b) { return (v8d)_mm512_add_pd((__m512d)a, (__m512d)b); }
static inline v8d sub(v8d a, v8d b) { return (v8d)_mm512_sub_pd((__m512d)a, (__m512d)b); }
static inline v8d mul(v8d a, v8d b) { return (v8d)_mm512_mul_pd((__m512d)a, (__m512d)b); }
static inline v8d bc(double d) { return (v8d)_mm512_set1_pd(d); }
static inline v16i iadd(v16i a, v16i b) { return (v16i)_mm512_add_epi32((__m512i)a, (__m512i)b); }
static inline v16i iand(v16i a, v16i b) { return (v16i)_mm512_and_epi32((__m512i)a, (__m512i)b); }
static inline v16i ibc(int i) { return (v16i)_mm512_set1_epi32(i); }

/* floor, exactly (vrndfxpntpd, rounding toward minus infinity: imm 1, checked on the card) */
static inline v8d vfloor(v8d x) { return __builtin_ia32_knc_vrndfxpntpd_zzi((v8d){ 0 }, 0xFF, x, 1, 0); }

/* two vectors of 8 integral doubles to one vector of 16 dwords */
static inline v16i to_i16(v8d lo, v8d hi)
{
    v16i a = __builtin_ia32_knc_vcvtfxpntpd2dq_zzi((v16i){ 0 }, 0xFFFF, lo, 0, 0);
    v16i b = __builtin_ia32_knc_vcvtfxpntpd2dq_zzi((v16i){ 0 }, 0xFFFF, hi, 0, 0);
    int A[16] A64, B[16] A64;
    memcpy(A, &a, 64);
    memcpy(B, &b, 64);
    memcpy(A + 8, B, 32);
    v16i r;
    memcpy(&r, A, 64);
    return r;
}

/* kncgather.S: out[i] = table[idx[i]] for 16 lanes, a call so that the compiler saves its live vector registers */
void knc_gather16(int32_t *out, const int32_t *table, const int32_t *idx);

static inline v16i gather(const int32_t *p, v16i idx)
{
    int ib[16] A64, ob[16] A64;
    memcpy(ib, &idx, 64);
    knc_gather16(ob, p, ib);
    v16i r;
    memcpy(&r, ob, 64);
    return r;
}

/* table[h & 15] for 16 lanes as two vectors of 8 doubles (vpermd from the float table, then exact conversion) */
static inline void lookup(const float *table, v16i h15, v8d out[2])
{
    v16i f = __builtin_ia32_knc_vpermd_zzm((v16i){ 0 }, 0xFFFF, h15, table, 0);
    v16f ff;
    memcpy(&ff, &f, 64);
    out[0] = __builtin_ia32_knc_vcvtps2pd_zz((v8d){ 0 }, 0xFF, ff, 0);
    v16f hi = __builtin_ia32_knc_vpermf32x4_zzi((v16f){ 0 }, 0xFFFF, ff, 0xEE);
    out[1] = __builtin_ia32_knc_vcvtps2pd_zz((v8d){ 0 }, 0xFF, hi, 0);
}

/* the gradients of a hash vector: GX[h], GY[h], GZ[h] for both halves */
typedef struct {
    v8d x[2], y[2], z[2];
} grads;

static inline void gradients(v16i hash, grads *g)
{
    v16i h = iand(hash, ibc(15));
    lookup(GXF, h, g->x);
    lookup(GYF, h, g->y);
    lookup(GZF, h, g->z);
}

/* grad(h, x, y, z) = GX[h] * x + GY[h] * y + GZ[h] * z, Java's order */
static inline v8d grad(const grads *g, int half, v8d x, v8d y, v8d z)
{
    return add(add(mul(g->x[half], x), mul(g->y[half], y)), mul(g->z[half], z));
}

static inline v8d lerp(v8d t, v8d a, v8d b) { return add(a, mul(t, sub(b, a))); }
static inline v8d vfade(v8d d)
{
    return mul(mul(mul(d, d), d), add(mul(d, sub(mul(d, bc(6.0)), bc(15.0))), bc(10.0)));
}

static inline double sfade(double d) { return d * d * d * (d * (d * 6.0 - 15.0) + 10.0); }
static inline int sfloor(double d)
{
    int i = (int)d;
    return d < (double)i ? i - 1 : i;
}
static inline int64_t lfloor(double d)
{
    int64_t i = (int64_t)d;
    return d < (double)i ? i - 1 : i;
}

/*
 * One 3D generator (o->n octaves) for the 25 columns of one chunk, 33 heights each, as populateNoiseArray called
 * by generateNoiseOctaves: out is zeroed, then every octave is added in order.
 */
static void octaves3d(const octaves *g, int x, int z, double xsc, double ysc, double zsc, double out[825])
{
    /* lanes: column c = lx * 5 + lz for lane c (0 to 24); lanes 25 to 31 repeat column 24 and are dropped */
    double LX[32] A64, LZ[32] A64;
    for (int c = 0; c < 32; c++) {
        int cc = c < 25 ? c : 24;
        LX[c] = (double)(cc / 5);
        LZ[c] = (double)(cc % 5);
    }
    v8d acc[2][33][2];
    memset(acc, 0, sizeof acc);

    double d6 = 1.0;
    for (int oc = 0; oc < g->n; ++oc) {
        const octave *o = &g->o[oc];
        /* generateNoiseOctaves' offsets for this octave, exactly as the scalar port */
        double d3 = (double)x * d6 * xsc;
        double d5 = (double)z * d6 * zsc;
        int64_t i2 = lfloor(d3), j2 = lfloor(d5);
        d3 -= (double)i2;
        d5 -= (double)j2;
        i2 %= 16777216LL;
        j2 %= 16777216LL;
        d3 += (double)i2;
        d5 += (double)j2;
        double xs = xsc * d6, ys = ysc * d6, zs = zsc * d6, d7 = 1.0 / d6;
        const int32_t *p = o->p;

        for (int grp = 0; grp < 2; grp++) {
            v8d dx[2], tx[2], dz[2], tz[2], fx[2], fz[2];
            for (int h = 0; h < 2; h++) {
                v8d lx, lz;
                memcpy(&lx, LX + grp * 16 + h * 8, 64);
                memcpy(&lz, LZ + grp * 16 + h * 8, 64);
                /* p2 + (double)l1 * p11 + xCoord */
                v8d X = add(add(bc(d3), mul(lx, bc(xs))), bc(o->x));
                v8d Z = add(add(bc(d5), mul(lz, bc(zs))), bc(o->z));
                fx[h] = vfloor(X);
                fz[h] = vfloor(Z);
                dx[h] = sub(X, fx[h]);
                dz[h] = sub(Z, fz[h]);
                tx[h] = vfade(dx[h]);
                tz[h] = vfade(dz[h]);
            }
            v16i j2v = iand(to_i16(fx[0], fx[1]), ibc(255));
            v16i i3v = iand(to_i16(fz[0], fz[1]), ibc(255));
            v16i pj2 = gather(p, j2v), pj21 = gather(p, iadd(j2v, ibc(1)));

            int i1 = -1;
            v8d d8[2], d9[2], d10[2], d11[2];
            for (int j3 = 0; j3 < 33; ++j3) {
                double d16 = 0.0 + (double)j3 * ys + o->y;
                int k3 = sfloor(d16);
                int l3 = k3 & 255;
                d16 -= (double)k3;
                double d17 = sfade(d16);
                if (j3 == 0 || l3 != i1) {
                    /* the shared-height cache: recomputed at the first height of each vertical cell, with its d16 */
                    i1 = l3;
                    v16i k4 = iadd(pj2, ibc(l3)), j5 = iadd(pj21, ibc(l3));
                    v16i l4 = iadd(gather(p, k4), i3v), i5 = iadd(gather(p, iadd(k4, ibc(1))), i3v);
                    v16i k5 = iadd(gather(p, j5), i3v), l5 = iadd(gather(p, iadd(j5, ibc(1))), i3v);
                    grads g1, g2, g3, g4, g5, g6, g7, g8;
                    gradients(gather(p, l4), &g1);
                    gradients(gather(p, k5), &g2);
                    gradients(gather(p, i5), &g3);
                    gradients(gather(p, l5), &g4);
                    gradients(gather(p, iadd(l4, ibc(1))), &g5);
                    gradients(gather(p, iadd(k5, ibc(1))), &g6);
                    gradients(gather(p, iadd(i5, ibc(1))), &g7);
                    gradients(gather(p, iadd(l5, ibc(1))), &g8);
                    v8d y0 = bc(d16), y1 = bc(d16 - 1.0);
                    for (int h = 0; h < 2; h++) {
                        v8d x0 = dx[h], x1 = sub(dx[h], bc(1.0)), z0 = dz[h], z1 = sub(dz[h], bc(1.0));
                        d8[h] = lerp(tx[h], grad(&g1, h, x0, y0, z0), grad(&g2, h, x1, y0, z0));
                        d9[h] = lerp(tx[h], grad(&g3, h, x0, y1, z0), grad(&g4, h, x1, y1, z0));
                        d10[h] = lerp(tx[h], grad(&g5, h, x0, y0, z1), grad(&g6, h, x1, y0, z1));
                        d11[h] = lerp(tx[h], grad(&g7, h, x0, y1, z1), grad(&g8, h, x1, y1, z1));
                    }
                }
                v8d t17 = bc(d17), s7 = bc(d7);
                for (int h = 0; h < 2; h++) {
                    v8d d18 = lerp(t17, d8[h], d9[h]);
                    v8d d19 = lerp(t17, d10[h], d11[h]);
                    v8d d20 = lerp(tz[h], d18, d19);
                    acc[grp][j3][h] = add(acc[grp][j3][h], mul(d20, s7));
                }
            }
        }
        d6 /= 2.0;
    }

    /* lane c of the accumulators is column c: out[c * 33 + y] */
    for (int grp = 0; grp < 2; grp++)
        for (int j3 = 0; j3 < 33; j3++) {
            double v[16] A64;
            memcpy(v, &acc[grp][j3][0], 64);
            memcpy(v + 8, &acc[grp][j3][1], 64);
            for (int lane = 0; lane < 16; lane++) {
                int c = grp * 16 + lane;
                if (c < 25) out[c * 33 + j3] = v[lane];
            }
        }
}

void vshape_octaves3d(const world *w, int cx, int cz, double o1[825], double o2[825], double o3[825])
{
    int x = cx * 4, z = cz * 4;
    octaves3d(&w->gen[1], x, z, 8.555150000000001, 4.277575000000001, 8.555150000000001, o1);
    octaves3d(&w->gen[2], x, z, 684.412, 684.412, 684.412, o2);
    octaves3d(&w->gen[3], x, z, 684.412, 684.412, 684.412, o3);
}
