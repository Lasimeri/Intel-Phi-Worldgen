/* rwg.c: RWG alpha 1.5.2's height stage in C, operation for operation as the decompiled Java (rwg.util.PerlinNoise,
 * rwg.util.CellNoise, rwg.world.ChunkManagerRealistic, rwg.world.ChunkGeneratorRealistic.getNewNoise, rwg.terrain.*,
 * the inline rNoise of the coast, island and test biomes). See rwg.md. */
#include "rwg.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* Rounds to float through memory (lib/shape.c): on the card's x87 at 53-bit precision control, one +, -, * or /
 * of floats then this store gives the IEEE single result. Every float operation below goes through it. */
static inline float rf(float v)
{
    volatile float t = v;
    return t;
}
#define FA(a, b) rf((a) + (b))
#define FS(a, b) rf((a) - (b))
#define FM(a, b) rf((a) * (b))
#define FD(a, b) rf((a) / (b))
/* (float) of an int, and Java's float to int cast: toward zero, NaN to 0, saturating */
#define FI(i) rf((float)(i))
static inline int jint(float f)
{
    if (f != f) return 0;
    if (f >= 2147483648.0f) return 2147483647;
    if (f <= -2147483648.0f) return -2147483647 - 1;
    return (int)f;
}
static inline int jintd(double d)
{
    if (d != d) return 0;
    if (d >= 2147483648.0) return 2147483647;
    if (d <= -2147483649.0) return -2147483647 - 1;
    return (int)d;
}

/* ---- PerlinNoise ---- */

static inline float scurve(float t) { return FM(FM(t, t), FS(3.0f, FM(2.0f, t))); }
static inline float flerp(float t, float a, float b) { return FA(a, FM(t, FS(b, a))); }

float rwg_noise1(const rwg_perlin *p, float x)
{
    float t = fabsf(FA(x, 4096.0f));
    int bx0 = jint(t) & 255, bx1 = (bx0 + 1) & 255;
    float rx0 = FS(t, FI(jint(t))), rx1 = FS(rx0, 1.0f);
    float sx = scurve(rx0);
    float u = FM(rx0, p->g1[p->p[bx0]]);
    float v = FM(rx1, p->g1[p->p[bx1]]);
    return flerp(sx, u, v);
}

float rwg_noise2(const rwg_perlin *p, float x, float y)
{
    float t = fabsf(FA(x, 4096.0f));
    int bx0 = jint(t) & 255, bx1 = (bx0 + 1) & 255;
    float rx0 = FS(t, FI(jint(t))), rx1 = FS(rx0, 1.0f);
    t = fabsf(FA(y, 4096.0f));
    int by0 = jint(t) & 255, by1 = (by0 + 1) & 255;
    float ry0 = FS(t, FI(jint(t))), ry1 = FS(ry0, 1.0f);
    int i = p->p[bx0], j = p->p[bx1];
    int b00 = p->p[i + by0], b10 = p->p[j + by0], b01 = p->p[i + by1], b11 = p->p[j + by1];
    float sx = scurve(rx0), sy = scurve(ry0);
    const float *g = p->g2;
    float u = FA(FM(rx0, g[b00 * 2]), FM(ry0, g[b00 * 2 + 1]));
    float v = FA(FM(rx1, g[b10 * 2]), FM(ry0, g[b10 * 2 + 1]));
    float a = flerp(sx, u, v);
    u = FA(FM(rx0, g[b01 * 2]), FM(ry1, g[b01 * 2 + 1]));
    v = FA(FM(rx1, g[b11 * 2]), FM(ry1, g[b11 * 2 + 1]));
    float b = flerp(sx, u, v);
    return flerp(sy, a, b);
}

/* ---- CellNoise ---- */

static double value2(int x, int z, int64_t seed)
{
    /* (long)(1619 * x + 6971 * z): an int expression, wrapping, then widened */
    int32_t xz = (int32_t)((uint32_t)x * 1619u + (uint32_t)z * 6971u);
    int64_t n = (int64_t)(((uint64_t)(int64_t)xz + 1013ull * (uint64_t)seed) & 2147483647ull);
    n ^= n >> 13;
    uint64_t un = (uint64_t)n;
    int64_t m = (int64_t)((un * (un * un * 60493ull + 19990303ull) + 1376312589ull) & 2147483647ull);
    return 1.0 - (double)m / 1.073741824E9;
}

static inline int cfloor(double v) { return v > 0.0 ? jintd(v) : jintd(v) - 1; }

/* (int) Math.floor(v) without libm: the card's musl floor() adds 2^63 and needs the x87 at 64-bit precision, the
 * card runs at 53 (Java's double) */
static inline int jfloor(double v)
{
    int t = jintd(v);
    return v == v && v < 2147483648.0 && v >= -2147483648.0 && (double)t > v ? t - 1 : t;
}

static inline double dist2d(const rwg_cell *c, double xd, double zd)
{
    if (c->method == 0) return sqrt(xd * xd + zd * zd) / 1.4142135623730951;
    if (c->method == 1) return xd + zd;
    return NAN;
}

float rwg_cell_noise(const rwg_cell *c, double x, double z, double frequency)
{
    x *= frequency;
    z *= frequency;
    int xi = cfloor(x), zi = cfloor(z);
    double min = 3.2E7, xc = 0.0, zc = 0.0;
    for (int zz = zi - 2; zz <= zi + 2; zz++) {
        for (int xx = xi - 2; xx <= xi + 2; xx++) {
            double xp = (double)xx + value2(xx, zz, c->seed);
            double zp = (double)zz + value2(xx, zz, c->offset);
            double xd = xp - x, zd = zp - z;
            double d = xd * xd + zd * zd;
            if (d < min) {
                min = d;
                xc = xp;
                zc = zp;
            }
        }
    }
    if (c->use_distance) return rf((float)dist2d(c, xc - x, zc - z));
    return rf((float)value2(jfloor(xc), jfloor(zc), c->seed));
}

float rwg_cell_border(const rwg_cell *c, double x, double z, double width, float depth)
{
    x *= 1.0;
    z *= 1.0;
    int xi = cfloor(x), zi = cfloor(z);
    double dc = 3.2E7, dn = 3.2E7;
    for (int zz = zi - 2; zz <= zi + 2; zz++) {
        for (int xx = xi - 2; xx <= xi + 2; xx++) {
            double xp = (double)xx + value2(xx, zz, c->seed);
            double zp = (double)zz + value2(xx, zz, c->offset);
            double d = dist2d(c, xp - x, zp - z);
            if (d < dc) {
                dn = dc;
                dc = d;
            } else if (d < dn) {
                dn = d;
            }
        }
    }
    double diff = dn - dc;
    if (diff < width) return FM(FS(rf((float)(diff / width)), 1.0f), depth);
    return 0.0f;
}

/* ---- ChunkManagerRealistic ---- */

static int diff(float s1, float s2, float base) { return (s1 < base && s2 > base) || (s1 > base && s2 < base); }

float rwg_ocean(const rwg_world *w, int x, int y)
{
    const rwg_perlin *p = &w->perlin;
    float base = 0.0f;
    float s1 = FA(rwg_noise2(p, FD(FI(x), 1200.0f), FD(FI(y), 1200.0f)), base);
    float s2, sa, highest = 0.0f;
    if (s1 == 0.0f) highest = 1.0f;
    if (diff(s1, s2 = FA(rwg_noise2(p, FD(FS(FI(x), 100.0f), 1200.0f), FD(FI(y), 1200.0f)), base), base)) {
        sa = FM(s1, FD(1.0f, fabsf(FS(s1, s2))));
        highest = FS(1.0f, fabsf(sa)) > highest ? FS(1.0f, fabsf(sa)) : highest;
    } else if (diff(s1, s2 = FA(rwg_noise2(p, FD(FA(FI(x), 100.0f), 1200.0f), FD(FI(y), 1200.0f)), base), base)) {
        sa = FM(s1, FD(1.0f, fabsf(FS(s1, s2))));
        highest = FS(1.0f, fabsf(sa)) > highest ? FS(1.0f, fabsf(sa)) : highest;
    }
    if (diff(s1, s2 = FA(rwg_noise2(p, FD(FI(x), 1200.0f), FD(FA(FI(y), 100.0f), 1200.0f)), base), base)) {
        sa = FM(s1, FD(1.0f, fabsf(FS(s1, s2))));
        highest = FS(1.0f, fabsf(sa)) > highest ? FS(1.0f, fabsf(sa)) : highest;
    } else if (diff(s1, s2 = FA(rwg_noise2(p, FD(FI(x), 1200.0f), FD(FS(FI(y), 100.0f), 1200.0f)), base), base)) {
        sa = FM(s1, FD(1.0f, fabsf(FS(s1, s2))));
        highest = FS(1.0f, fabsf(sa)) > highest ? FS(1.0f, fabsf(sa)) : highest;
    }
    if (s1 > 0.0f) highest = FS(2.0f, highest);
    return highest;
}

static inline float clamp1(float h) { return h < 0.0f ? 0.0f : (h >= 0.9999999f ? 0.9999999f : h); }

/* the list entry at noise h in [0, 1) */
static int pick(const rwg_world *w, int list, float h)
{
    h = FM(clamp1(h), FI(w->nlist[list]));
    return w->list[list][jint(h)];
}

/* getBiomeDataAt(x, z, ocean): the ocean value plays no part; its cache changes nothing */
int rwg_biome_at(const rwg_world *w, int x, int z)
{
    const rwg_cell *bc = &w->biomecell;
    float b = FA(FM(rwg_cell_noise(bc, (double)FA(FI(x), 4000.0f) / 1200.0, (double)z / 1200.0, 1.0), 0.5f), 0.5f);
    b = clamp1(b);
    float s = w->small_enabled ? FA(FM(rwg_cell_noise(bc, (double)x / 140.0, (double)z / 140.0, 1.0), 0.5f), 0.5f) : 0.0f;
    int wet = w->wet_enabled, list;
    if (w->small_enabled && s > 0.975f) return pick(w, 4, FM(FS(s, 0.975f), 40.0f));
    if ((!wet || b >= 0.25f) && (wet || b >= 0.33f)) {
        if ((!wet || b >= 0.5f) && (wet || b >= 0.66f)) {
            if ((!wet || b >= 0.75f) && (wet || b >= 1.0f)) list = wet ? 3 : 2;
            else list = 2;
        } else {
            list = 1;
        }
    } else {
        list = 0;
    }
    return pick(w, list, FA(FM(rwg_cell_noise(bc, (double)x / 450.0, (double)z / 450.0, 1.0), 0.5f), 0.5f));
}

static inline float warp(const rwg_perlin *p, int a, int b)
{
    /* (float)a + perlin.noise1((float)b / 240.0F) * 220.0F */
    return FA(FI(a), FM(rwg_noise1(p, FD(FI(b), 240.0f)), 220.0f));
}

float rwg_river_strength(const rwg_world *w, int x, int y)
{
    const rwg_perlin *p = &w->perlin;
    return rwg_cell_border(&w->cmr_cell, (double)warp(p, x, y) / 1250.0, (double)warp(p, y, x) / 1250.0,
                           0.16666666666666666, 1.0f);
}

static float calc_river(const rwg_world *w, int x, int y, float st, float h)
{
    if (!(st < 0.0f && h > 59.0f)) return h;
    const rwg_perlin *p = &w->perlin;
    float px = warp(p, x, y), py = warp(p, y, x);
    float r = rwg_cell_border(&w->cmr_cell, (double)px / 1250.0, (double)py / 1250.0, 0.038461538461538464, 1.0f);
    float n = FA(FA(59.0f, FM(rwg_noise2(p, FD(FI(x), 12.0f), FD(FI(y), 12.0f)), 2.0f)),
                 FM(rwg_noise2(p, FD(FI(x), 8.0f), FD(FI(y), 8.0f)), 1.5f));
    return FA(FM(h, FA(r, 1.0f)), FM(n, -r));
}

/* ---- rNoise: the terrains (rwg.terrain.*) and the inline biomes ---- */

/* perlin.noise2((float)x / d, (float)y / d) */
#define N2(d) rwg_noise2(p, FD(FI(x), (d)), FD(FI(y), (d)))
/* cell.noise((double)x / d, (double)y / d, 1.0D) */
#define CN(d) rwg_cell_noise(c, (double)x / (d), (double)y / (d), 1.0)
/* cell.noise((double)((float)x / d), ...): the division in float */
#define CNF(d) rwg_cell_noise(c, (double)FD(FI(x), (d)), (double)FD(FI(y), (d)), 1.0)

static float terrain(const rwg_biome *b, const rwg_perlin *p, const rwg_cell *c, int x, int y, float ocean,
                     float border, float river)
{
    const float *q = b->param;
    float h, m, l, sm, st, d, d2;
    (void)ocean;
    switch (b->kind) {
    case RT_BASE:
        return 70.0f;
    case RT_CANYON: {
        /* q: strength, base, smallRiver, n, height[n] */
        float strength = q[0], base = q[1];
        int small = q[2] != 0.0f, n = (int)q[3];
        const float *ht = q + 4;
        river = FM(river, 1.3f);
        river = river > 1.0f ? 1.0f : river;
        float r = FM(N2(100.0f), 50.0f);
        r = r < -7.4f ? -7.4f : (r > 7.4f ? 7.4f : r);
        float bb = FM(FA(17.0f, r), river);
        float hn = FM(N2(12.0f), 0.5f);
        float sb = 0.0f;
        if (bb > 0.0f) {
            sb = bb < 0.0f ? 0.0f : (bb > 7.0f ? 7.0f : bb);
            sb = FM(hn, sb);
        }
        bb = FA(bb, sb);
        float ctot = 0.0f, ctmp;
        for (int bn = 0; bn < n; bn += 2) {
            ctmp = 0.0f;
            if (bb > ht[bn] && border > FA(FA(0.6f, FM(ht[bn], 0.015f)), FM(hn, 0.2f))) {
                ctmp = bb > FA(ht[bn], ht[bn + 1]) ? ht[bn + 1] : FS(bb, ht[bn]);
                ctmp = FM(ctmp, strength);
            }
            ctot = FA(ctot, ctmp);
        }
        float v = 0.0f;
        if (small) {
            if (bb < 5.0f) {
                v = FS(5.0f, bb);
                for (int i = 0; i < 3; i++) v = FM(v, FD(v, 4.5f));
            }
        } else if (bb < 5.0f) {
            v = FM(FM(FA(FM(N2(7.0f), 1.3f), FM(N2(15.0f), 2.0f)), FS(5.0f, bb)), 0.2f);
        }
        bb = FA(bb, FS(ctot, v));
        return FA(base, bb);
    }
    case RT_DUNES:
        st = FM(FA(N2(160.0f), 0.38f), 35.0f);
        st = st < 0.2f ? 0.2f : st;
        h = FM(FM(N2(60.0f), st), 2.0f);
        h = h > 0.0f ? -h : h;
        h = FA(h, st);
        h = FM(h, FD(h, 50.0f));
        h = FA(h, st);
        if (h < 10.0f) {
            d = FD(FS(h, 10.0f), 2.0f);
            d = d > 4.0f ? 4.0f : d;
            h = FA(h, FM(CN(25.0), d));
            h = FA(h, FM(N2(30.0f), d));
            h = FA(h, FM(FM(N2(14.0f), d), 0.5f));
        }
        return FA(70.0f, FM(h, river));
    case RT_DUNEVALLEY: {
        float valley = q[0];
        h = FM(FM(FA(rwg_noise2(p, FD(FI(x), valley), FD(FI(y), valley)), 0.25f), 65.0f), river);
        h = h < 1.0f ? 1.0f : h;
        float r = FM(FM(CN(50.0), h), 2.0f);
        h = FA(h, r);
        h = FA(h, FM(N2(40.0f), 8.0f));
        h = FA(h, FM(N2(14.0f), 2.0f));
        return FA(70.0f, h);
    }
    case RT_FLATLAKES:
        h = FM(FM(N2(300.0f), 40.0f), river);
        h = h > 3.0f ? 3.0f : h;
        h = FA(h, FM(FM(N2(50.0f), FS(12.0f, h)), 0.4f));
        h = FA(h, FM(FM(N2(15.0f), FS(12.0f, h)), 0.15f));
        return FA(62.0f, h);
    case RT_GRASSFLATS:
        h = FM(N2(100.0f), 7.0f);
        h = FA(h, FM(N2(20.0f), 2.0f));
        m = FM(FM(N2(180.0f), 70.0f), river);
        m = FM(m, FD(m, 40.0f));
        sm = FM(N2(30.0f), 8.0f);
        sm = FM(sm, FD(m, 20.0f) > 3.75f ? 3.75f : FD(m, 20.0f));
        m = FA(m, sm);
        l = FM(N2(260.0f), 38.0f);
        l = FM(l, FD(l, 25.0f));
        l = l < -8.0f ? -8.0f : l;
        return FS(FA(FA(68.0f, h), m), l);
    case RT_GRASSHILLS: {
        /* q: hHeight, hWidth, vHeight, vWidth, lHeight, lWidth, bHeight */
        h = FM(FM(rwg_noise2(p, FD(FI(x), q[3]), FD(FI(y), q[3])), q[2]), river);
        h = FA(h, FM(N2(20.0f), 2.0f));
        m = FM(FM(rwg_noise2(p, FD(FI(x), q[1]), FD(FI(y), q[1])), q[0]), river);
        m = FM(m, FD(m, 40.0f));
        sm = FM(N2(30.0f), 8.0f);
        sm = FM(sm, FD(m, 20.0f) > 3.75f ? 3.75f : FD(m, 20.0f));
        m = FA(m, sm);
        float cm = FM(CN(25.0), 12.0f);
        cm = FM(cm, FD(m, 20.0f) > 3.75f ? 3.75f : FD(m, 20.0f));
        m = FA(m, cm);
        l = FM(rwg_noise2(p, FD(FI(x), q[5]), FD(FI(y), q[5])), q[4]);
        l = FM(l, FD(l, 25.0f));
        l = l < 8.0f ? 8.0f : l;
        h = FA(h, FM(N2(12.0f), 3.0f));
        h = FA(h, FM(N2(5.0f), 1.5f));
        return FS(FA(FA(q[6], h), m), l);
    }
    case RT_GRASSMOUNTAINS: {
        h = FM(N2(100.0f), 7.0f);
        h = FA(h, FM(N2(20.0f), 2.0f));
        m = FM(FM(N2(230.0f), 120.0f), river);
        m = FM(m, FD(m, 35.0f));
        m = m > 70.0f ? FA(70.0f, FD(FS(m, 70.0f), 2.5f)) : m;
        float cc = FM(CNF(30.0f), FM(m, 0.3f)); /* bytecode: noise * (m * 0.3F) */
        sm = FA(FM(N2(30.0f), 8.0f), N2(8.0f));
        sm = FM(sm, FD(m, 20.0f) > 2.5f ? 2.5f : FD(m, 20.0f));
        m = FA(m, sm);
        m = FA(m, cc);
        l = FM(N2(260.0f), 38.0f);
        l = FM(l, FD(l, 25.0f));
        l = l < -8.0f ? -8.0f : l;
        return FS(FA(FA(68.0f, h), m), l);
    }
    case RT_HIGHLAND: {
        /* q: start, height, base, width */
        h = FM(FM(rwg_noise2(p, FD(FI(x), q[3]), FD(FI(y), q[3])), q[1]), river);
        h = h < q[0] ? FA(q[0], FD(FS(h, q[0]), 4.5f)) : h;
        if (h > 0.0f) {
            st = FM(h, 1.5f) > 15.0f ? 15.0f : FM(h, 1.5f);
            h = FA(h, FM(CN(70.0), st));
        }
        h = FA(h, FM(N2(20.0f), 5.0f));
        h = FA(h, FM(N2(12.0f), 3.0f));
        h = FA(h, FM(N2(5.0f), 1.5f));
        return FA(q[2], h);
    }
    case RT_HILLY: {
        /* q: width, strength, lakeDepth, lakeWidth, terrainHeight */
        h = FM(N2(20.0f), 2.0f);
        h = FA(h, FM(N2(7.0f), 0.8f));
        m = FM(FM(rwg_noise2(p, FD(FI(x), q[0]), FD(FI(y), q[0])), q[1]), river);
        m = FM(m, FD(m, 35.0f));
        m = m > 70.0f ? FA(70.0f, FD(FS(m, 70.0f), 2.5f)) : m;
        st = FM(m, 0.7f);
        st = st > 20.0f ? 20.0f : st;
        float cc = FM(CNF(30.0f), FA(5.0f, st));
        sm = FA(FM(N2(30.0f), 8.0f), N2(8.0f));
        sm = FM(sm, FD(FA(m, 10.0f), 20.0f) > 2.5f ? 2.5f : FD(FA(m, 10.0f), 20.0f));
        m = FA(m, sm);
        m = FA(m, cc);
        l = FM(rwg_noise2(p, FD(FI(x), q[3]), FD(FI(y), q[3])), q[2]);
        l = FM(l, FD(l, 25.0f));
        l = l < -8.0f ? -8.0f : l;
        return FS(FA(FA(q[4], h), m), l);
    }
    case RT_MARSH:
        h = FM(N2(130.0f), 30.0f);
        h = FA(h, FM(N2(12.0f), 2.0f));
        h = FA(h, FM(N2(18.0f), 4.0f));
        h = h < 4.0f ? 0.0f : FS(h, 4.0f);
        if (h == 0.0f) h = FA(h, FA(N2(20.0f), N2(5.0f)));
        return FA(62.0f, h);
    case RT_MESA: {
        float bb = FM(FM(N2(130.0f), 50.0f), river);
        bb = FM(bb, FD(bb, 40.0f));
        float hn = N2(12.0f);
        float sb = 0.0f;
        if (bb > 2.0f) {
            sb = FD(FS(bb, 2.0f), 2.0f);
            sb = sb < 0.0f ? 0.0f : (sb > 5.5f ? 5.5f : sb);
            sb = FM(hn, sb);
        }
        bb = FA(bb, sb);
        bb = bb < 0.1f ? 0.1f : bb;
        float c1 = 0.0f;
        if (bb > 1.0f) {
            c1 = bb > 5.5f ? 4.5f : FS(bb, 1.0f);
            c1 = FM(c1, 3.0f);
        }
        float c2 = 0.0f;
        if (bb > 5.5f && border > FA(0.95f, FM(hn, 0.09f))) {
            c2 = bb > 6.0f ? 0.5f : FS(bb, 5.5f);
            c2 = FM(c2, 35.0f);
        }
        float bn = 0.0f, ww;
        if (bb < 7.0f) {
            ww = FS(5.0f, bb);
            bn = FA(bn, FM(N2(70.0f), FM(ww, 0.4f))); /* bytecode: noise * (w * 0.4F) */
            bn = FA(bn, FM(N2(20.0f), FM(ww, 0.3f)));
        }
        ww = FM(N2(80.0f), 25.0f);
        ww = FM(ww, FD(ww, 25.0f));
        bb = FA(bb, FS(FA(FA(c1, c2), bn), ww));
        return FA(74.0f, bb);
    }
    case RT_MOUNTAIN:
        h = FM(FM(N2(300.0f), 135.0f), river);
        h = FM(h, FD(h, 32.0f));
        h = h > 150.0f ? 150.0f : h;
        if (h > 10.0f) {
            d = FD(FS(h, 10.0f), 2.0f) > 8.0f ? 8.0f : FD(FS(h, 10.0f), 2.0f);
            h = FA(h, FM(N2(35.0f), d));
            h = FA(h, FM(FM(N2(60.0f), d), 0.5f));
            if (h > 35.0f) {
                d2 = FD(FS(h, 35.0f), 1.5f) > 30.0f ? 30.0f : FD(FS(h, 35.0f), 1.5f);
                h = FA(h, FM(CN(25.0), d2));
            }
        }
        h = FA(h, FM(N2(28.0f), 4.0f));
        h = FA(h, FM(N2(18.0f), 2.0f));
        h = FA(h, FM(N2(8.0f), 2.0f));
        return FA(h, 67.0f);
    case RT_MOUNTAINRIVER:
        h = FM(FM(N2(300.0f), 135.0f), river);
        h = FM(h, FD(h, 32.0f));
        h = h > 150.0f ? 150.0f : h;
        if (h < 10.0f) h = FA(h, FM(FM(N2(14.0f), FS(10.0f, h)), 0.2f));
        if (h > 10.0f) {
            d = FD(FS(h, 10.0f), 2.0f) > 8.0f ? 8.0f : FD(FS(h, 10.0f), 2.0f);
            h = FA(h, FM(N2(35.0f), d));
            h = FA(h, FM(FM(N2(60.0f), d), 0.5f));
            if (h > 35.0f) {
                d2 = FD(FS(h, 35.0f), 1.5f) > 30.0f ? 30.0f : FD(FS(h, 35.0f), 1.5f);
                h = FA(h, FM(CN(25.0), d2));
            }
        }
        if (h > 2.0f) {
            d = FD(FS(h, 2.0f), 2.0f) > 4.0f ? 4.0f : FD(FS(h, 2.0f), 2.0f);
            h = FA(h, FM(N2(28.0f), d));
            h = FA(h, FM(N2(18.0f), FD(d, 2.0f)));
            h = FA(h, FM(N2(8.0f), FD(d, 2.0f)));
        }
        return FA(h, 67.0f);
    case RT_MOUNTAINSPIKES: {
        float bb = FA(12.0f, FM(N2(300.0f), 6.0f));
        h = FM(FM(CN(200.0), bb), river);
        h = FM(h, FM(h, 1.5f));
        h = h > 155.0f ? 155.0f : h;
        if (h > 2.0f) {
            d = FD(FS(h, 2.0f), 2.0f) > 8.0f ? 8.0f : FD(FS(h, 2.0f), 2.0f);
            h = FA(h, FM(N2(30.0f), d));
            h = FA(h, FM(FM(N2(50.0f), d), 0.5f));
            if (h > 35.0f) {
                d2 = FD(FS(h, 35.0f), 1.5f) > 30.0f ? 30.0f : FD(FS(h, 35.0f), 1.5f);
                h = FA(h, FM(CN(25.0), d2));
            }
        }
        h = FA(h, FM(N2(18.0f), 3.0f));
        h = FA(h, FM(N2(8.0f), 2.0f));
        return FA(FA(45.0f, h), FM(bb, 2.0f));
    }
    case RT_POLAR:
        st = FM(FM(FA(N2(160.0f), 0.38f), 35.0f), river);
        st = st < 0.2f ? 0.2f : st;
        h = FM(FM(N2(60.0f), st), 2.0f);
        h = h > 0.0f ? -h : h;
        h = FA(h, st);
        h = FM(h, FD(h, 50.0f));
        h = FA(h, st);
        return FA(70.0f, h);
    case RT_SMALLSUPPORT:
        h = FM(N2(100.0f), 8.0f);
        h = FA(h, FM(N2(30.0f), 4.0f));
        h = FA(h, FM(N2(15.0f), 2.0f));
        h = FA(h, N2(7.0f));
        return FA(FA(70.0f, FM(20.0f, river)), h);
    case RT_SWAMPMOUNTAIN: {
        /* q: heigth, width */
        h = FM(FM(rwg_noise2(p, FD(FI(x), q[1]), FD(FI(y), q[1])), q[0]), river);
        h = FM(h, FD(h, 32.0f));
        h = h > 150.0f ? 150.0f : h;
        if (h < 14.0f) h = FA(h, FM(FM(N2(25.0f), FS(14.0f, h)), 0.8f));
        if (h < 6.0f) h = FA(FA(FS(6.0f, FM(FS(6.0f, h), 0.07f)), N2(20.0f)), N2(5.0f));
        if (h > 10.0f) {
            d = FD(FS(h, 10.0f), 2.0f) > 8.0f ? 8.0f : FD(FS(h, 10.0f), 2.0f);
            h = FA(h, FM(N2(35.0f), d));
            h = FA(h, FM(FM(N2(60.0f), d), 0.5f));
            if (h > 35.0f) {
                d2 = FD(FS(h, 35.0f), 1.5f) > 30.0f ? 30.0f : FD(FS(h, 35.0f), 1.5f);
                h = FA(h, FM(CN(25.0), d2));
            }
        }
        if (h > 2.0f) {
            d = FD(FS(h, 2.0f), 2.0f) > 4.0f ? 4.0f : FD(FS(h, 2.0f), 2.0f);
            h = FA(h, FM(N2(28.0f), d));
            h = FA(h, FM(N2(18.0f), FD(d, 2.0f)));
            h = FA(h, FM(N2(8.0f), FD(d, 2.0f)));
        }
        return FA(h, 56.0f);
    }
    case RT_SWAMPRIVER:
        h = FM(FM(N2(180.0f), 40.0f), river);
        h = FM(h, FD(h, 35.0f));
        if (h < 1.0f) h = 1.0f;
        if (h < 4.0f) h = FA(h, FM(FA(N2(50.0f), N2(15.0f)), FS(4.0f, h)));
        return FA(60.0f, h);
    }
    return NAN; /* a kind RwgCapture never writes */
}

/* the start of the cold-slope, ice, oasis and test coasts: noise at 90, 40 and 9 */
static inline float coast_start(const rwg_perlin *p, int x, int y)
{
    return FA(FA(FM(N2(90.0f), 1.0f), FM(N2(40.0f), 0.15f)), FM(N2(9.0f), 0.07f));
}

static float rnoise(const rwg_biome *b, const rwg_perlin *p, const rwg_cell *c, int x, int y, float ocean,
                    float border, float river)
{
    float h, cc, st, start;
    switch (b->kind) {
    case RI_BASE:
        return 63.0f;
    case RI_TEST:
        return 70.0f;
    case RI_TESTRIVER:
        return 64.1f;
    case RI_OCEANTEST:
        return 45.0f;
    case RI_COASTCOLDCLIFF:
    case RI_COASTMANGROVE:
        h = ocean < 0.5f ? FM(ocean, 14.0f) : 7.0f;
        if (ocean < 1.9f) {
            h = FA(h, FM(N2(12.0f), 1.5f));
            h = FA(h, FM(N2(20.0f), 3.0f));
        }
        return FA(55.0f, h);
    case RI_COASTCOLDSLOPE:
        river = river > 0.5f ? 1.0f : FM(river, 2.0f);
        start = coast_start(p, x, y);
        h = ocean < 1.0f ? FM(ocean, 15.0f) : 15.0f;
        cc = 0.0f;
        if (FA(ocean, start) > 0.8f) {
            cc = FA(ocean, start) > 1.5f ? 0.7f : FS(FA(ocean, start), 0.8f);
            cc = FM(cc, FA(35.0f, FM(N2(50.0f), 8.0f)));
        }
        if (ocean < 1.6f) {
            h = FA(h, N2(12.0f));
            h = FA(h, FM(N2(20.0f), 2.0f));
        }
        return FA(FA(55.0f, h), FM(cc, river));
    case RI_COASTDUNES:
        h = ocean < 0.5f ? FM(ocean, 18.0f) : 9.0f;
        if (ocean < 1.9f) {
            h = FA(h, FM(N2(12.0f), 1.0f));
            h = FA(h, FM(N2(23.0f), 2.0f));
        }
        if (ocean > 1.0f) {
            h = FA(h, FM(FM(N2(15.0f), FS(ocean, 1.0f)), 8.0f));
            h = FA(h, FM(FM(N2(25.0f), FS(ocean, 1.0f)), 13.0f));
            h = FA(h, FM(FS(ocean, 1.0f), 9.0f));
        }
        return FA(55.0f, h);
    case RI_COASTICE:
        river = river > 0.5f ? 1.0f : FM(river, 2.0f);
        start = coast_start(p, x, y);
        h = 0.0f;
        cc = 0.0f;
        if (FA(ocean, start) > 1.4f) {
            cc = FA(ocean, start) > 1.5f ? 0.1f : FS(FA(ocean, start), 1.4f);
            cc = FM(cc, FA(250.0f, FM(N2(50.0f), 25.0f)));
        }
        if (ocean < 1.3f) {
            h = FA(h, N2(12.0f));
            h = FA(h, FM(N2(20.0f), 2.0f));
        }
        return FA(FA(55.0f, h), FM(cc, river));
    case RI_COASTOASIS:
    case RI_COASTTEST:
        river = river > 0.5f ? 1.0f : FM(river, 2.0f);
        start = coast_start(p, x, y);
        h = ocean < 1.0f ? FM(ocean, 10.0f) : 10.0f;
        cc = 0.0f;
        if (FA(ocean, start) > 1.0f) {
            cc = FA(ocean, start) > 1.2f ? 0.2f : FS(FA(ocean, start), 1.0f);
            cc = FM(cc, FA(100.0f, FM(N2(50.0f), 55.0f)));
        }
        if (ocean < 1.2f) {
            h = FA(h, N2(12.0f));
            h = FA(h, FM(N2(20.0f), 2.0f));
        }
        return FA(FA(55.0f, h), FM(cc, river));
    case RI_ISLANDTROPICAL: {
        st = FS(15.0f, FA(FM(CN(500.0), 42.0f), FM(N2(30.0f), 2.0f)));
        st = st < 0.0f ? 0.0f : st;
        h = st < 0.0f ? 0.0f : st;
        /* bytecode: h += (h * 0.4F) * ((h * 0.4F) * 2.0F); the decompiled text drops those parentheses */
        h = FA(h, FM(FM(h, 0.4f), FM(FM(h, 0.4f), 2.0f)));
        if (h > 10.0f) {
            float d2 = FD(FS(h, 10.0f), 1.5f) > 30.0f ? 30.0f : FD(FS(h, 10.0f), 1.5f);
            h = FA(h, FM(CN(25.0), d2));
        }
        h = FA(h, FM(N2(18.0f), 3.0f));
        h = FA(h, FM(N2(8.0f), 2.0f));
        return FA(55.0f, FM(h, border));
    }
    case RI_ISLANDTUNDRA:
        h = 0.0f;
        if (border > 0.9f) h = FM(FS(border, 0.9f), FA(150.0f, FM(N2(45.0f), 50.0f)));
        return FA(66.0f, h);
    }
    return terrain(b, p, c, x, y, ocean, border, river);
}

/* ---- getNewNoise ---- */

/* Over the biome ids the chunk's samples hold, ascending: every other id is 0 in every grid, which mix4 and the
 * column loop skip anyway, so leaving them out changes no operation and no order. */
static void mix4(float *const ing[4], float *res, const int *act, int nact)
{
    for (int a = 0; a < nact; a++) res[act[a]] = 0.0f;
    for (int a = 0; a < nact; a++) {
        int i = act[a];
        for (int j = 0; j < 4; j++)
            if (ing[j][i] > 0.0f) res[i] = FA(res[i], FD(ing[j][i], 4.0f));
    }
}

void rwg_new_noise(const rwg_world *w, rwg_scratch *s, int x, int y, rwg_heights *out)
{
    float (*H)[256] = s->huge, (*S)[256] = s->small;
    int *bd = s->biome_data;
    for (int i = -8; i < 13; i++)
        for (int j = -8; j < 13; j++) bd[(i + 8) * 21 + j + 8] = rwg_biome_at(w, x + (i * 8 - 8), y + (j * 8 - 8));
    int act[256], nact = 0;
    {
        uint8_t seen[256] = { 0 };
        for (int i = 0; i < 441; i++) seen[bd[i] & 255] = 1;
        for (int k = 0; k < 256; k++)
            if (seen[k]) act[nact++] = k;
    }
    for (int i = -1; i < 4; i++) {
        for (int j = -1; j < 4; j++) {
            float *h = H[(i * 2 + 2) * 9 + j * 2 + 2];
            for (int a = 0; a < nact; a++) h[act[a]] = 0.0f;
            for (int k = -8; k <= 8; k++)
                for (int l = -8; l <= 8; l++) {
                    int id = bd[(i + k + 8 + 1) * 21 + j + l + 8 + 1];
                    h[id] = FA(h[id], w->parabolic[k + 8 + (l + 8) * 17]);
                }
        }
    }
    int fixed = -1;
    for (int a = 0; a < nact; a++)
        if (H[40][act[a]] > 0.95f) fixed = act[a];
    float *ing[4];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            ing[0] = H[i * 2 * 9 + j * 2];
            ing[1] = H[(i * 2 + 2) * 9 + j * 2];
            ing[2] = H[i * 2 * 9 + j * 2 + 2];
            ing[3] = H[(i * 2 + 2) * 9 + j * 2 + 2];
            mix4(ing, H[(i * 2 + 1) * 9 + j * 2 + 1], act, nact);
        }
    for (int i = 0; i < 7; i++)
        for (int j = 0; j < 7; j++) {
            if ((i % 2 != 0 || j % 2 != 0) && (i % 2 == 0 || j % 2 == 0)) {
                ing[0] = H[i * 9 + j + 1];
                ing[1] = H[(i + 1) * 9 + j];
                ing[2] = H[(i + 1) * 9 + j + 2];
                ing[3] = H[(i + 2) * 9 + j + 1];
                mix4(ing, S[i * 4 * 25 + j * 4], act, nact);
            } else {
                for (int a = 0; a < nact; a++) S[i * 4 * 25 + j * 4][act[a]] = H[(i + 1) * 9 + j + 1][act[a]];
            }
        }
    for (int i = 0; i < 6; i++)
        for (int j = 0; j < 6; j++) {
            ing[0] = S[i * 4 * 25 + j * 4];
            ing[1] = S[(i * 4 + 4) * 25 + j * 4];
            ing[2] = S[i * 4 * 25 + j * 4 + 4];
            ing[3] = S[(i * 4 + 4) * 25 + j * 4 + 4];
            mix4(ing, S[(i * 4 + 2) * 25 + j * 4 + 2], act, nact);
        }
    for (int i = 0; i < 11; i++)
        for (int j = 0; j < 11; j++)
            if ((i % 2 != 0 || j % 2 != 0) && (i % 2 == 0 || j % 2 == 0)) {
                ing[0] = S[i * 2 * 25 + j * 2 + 2];
                ing[1] = S[(i * 2 + 2) * 25 + j * 2];
                ing[2] = S[(i * 2 + 2) * 25 + j * 2 + 4];
                ing[3] = S[(i * 2 + 4) * 25 + j * 2 + 2];
                mix4(ing, S[(i * 2 + 2) * 25 + j * 2 + 2], act, nact);
            }
    for (int i = 0; i < 9; i++)
        for (int j = 0; j < 9; j++) {
            ing[0] = S[(i * 2 + 2) * 25 + j * 2 + 2];
            ing[1] = S[(i * 2 + 4) * 25 + j * 2 + 2];
            ing[2] = S[(i * 2 + 2) * 25 + j * 2 + 4];
            ing[3] = S[(i * 2 + 4) * 25 + j * 2 + 4];
            mix4(ing, S[(i * 2 + 3) * 25 + j * 2 + 3], act, nact);
        }
    for (int i = 0; i < 16; i++)
        for (int j = 0; j < 16; j++)
            if ((i % 2 != 0 || j % 2 != 0) && (i % 2 == 0 || j % 2 == 0)) {
                ing[0] = S[(i + 3) * 25 + j + 4];
                ing[1] = S[(i + 4) * 25 + j + 3];
                ing[2] = S[(i + 4) * 25 + j + 5];
                ing[3] = S[(i + 5) * 25 + j + 4];
                mix4(ing, S[(i + 4) * 25 + j + 4], act, nact);
            }

    int rand_biome = fixed < 0;
    float bcount = 0.0f, brand = 0.0f;
    for (int i = 0; i < 256; i++) out->biome[i] = (int16_t)fixed;
    for (int i = 0; i < 256; i++) out->mapgen[i] = 0.0f;
    const rwg_perlin *p = &w->perlin;
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            if (rand_biome) {
                bcount = 0.0f;
                brand = FA(0.5f, rwg_noise2(p, FD(FI(x + i), 15.0f), FD(FI(y + j), 15.0f)));
                brand = brand < 0.0f ? 0.0f : (brand > 0.99999f ? 0.99999f : brand);
            }
            float ocean = rwg_ocean(w, x + i, y + j);
            int l = (i + 4) * 25 + j + 4;
            float th = 0.0f;
            float river = rwg_river_strength(w, x + i, y + j);
            out->river[i * 16 + j] = river;
            if (l == 312) {
                out->mapgen[256] = ocean;
                out->mapgen[257] = river;
            }
            for (int a = 0; a < nact; a++) {
                int k = act[a];
                float wk = S[l][k];
                if (!(wk > 0.0f)) continue;
                if (rand_biome && bcount <= 1.0f) {
                    bcount = FA(bcount, wk);
                    if (bcount > brand) {
                        out->biome[j * 16 + i] = (int16_t)k;
                        bcount = 2.0f;
                    }
                }
                if (l == 312) out->mapgen[k] = wk;
                float rn = rnoise(&w->biome[k], p, &w->cell, x + i, y + j, ocean, wk, FA(river, 1.0f));
                th = FA(th, FM(calc_river(w, x + i, y + j, river, rn), wk));
            }
            out->height[i * 16 + j] = th;
        }
    }
}

/* ---- loading ---- */

int rwg_load(rwg_world *w, const capfile *f, const char *what)
{
    uint32_t n;
    memset(w, 0, sizeof *w);
    const uint8_t *q = cap_section(f, "SEED", &n);
    if (!q || n < 8) {
        fprintf(stderr, "%s: no SEED section\n", what);
        return -1;
    }
    w->seed = rd64(q);
    q = cap_section(f, "NOIS", &n);
    if (!q || n != 4u * (8194 + 8194 + 16388 + 24582)) {
        fprintf(stderr, "%s: NOIS is %u bytes\n", what, q ? n : 0);
        return -1;
    }
    for (int i = 0; i < 8194; i++, q += 4) w->perlin.p[i] = rd32(q);
    for (int i = 0; i < 8194; i++, q += 4) w->perlin.g1[i] = rdf32(q);
    for (int i = 0; i < 16388; i++, q += 4) w->perlin.g2[i] = rdf32(q);
    for (int i = 0; i < 24582; i++, q += 4) w->perlin.g3[i] = rdf32(q);
    q = cap_section(f, "CELL", &n);
    if (!q || n != 3 * 19) {
        fprintf(stderr, "%s: CELL is %u bytes\n", what, q ? n : 0);
        return -1;
    }
    rwg_cell *cells[3] = { &w->cell, &w->cmr_cell, &w->biomecell };
    for (int i = 0; i < 3; i++, q += 19) {
        cells[i]->seed = rd64(q);
        cells[i]->offset = rd64(q + 8);
        cells[i]->use_distance = q[16];
        cells[i]->method = (int16_t)rd16(q + 17);
    }
    q = cap_section(f, "PARB", &n);
    if (!q || n != 289 * 4) {
        fprintf(stderr, "%s: PARB is %u bytes\n", what, q ? n : 0);
        return -1;
    }
    for (int i = 0; i < 289; i++) w->parabolic[i] = rdf32(q + 4 * i);
    q = cap_section(f, "LIST", &n);
    const uint8_t *end = q + n;
    if (!q || n < 2) {
        fprintf(stderr, "%s: no LIST section\n", what);
        return -1;
    }
    w->wet_enabled = q[0];
    w->small_enabled = q[1];
    q += 2;
    for (int l = 0; l < 6; l++) {
        if (q + 4 > end) return fprintf(stderr, "%s: LIST short\n", what), -1;
        int c = rd32(q);
        q += 4;
        if (c < 0 || c > 256 || q + 2 * c > end) return fprintf(stderr, "%s: LIST %d has %d\n", what, l, c), -1;
        w->nlist[l] = c;
        for (int i = 0; i < c; i++, q += 2) w->list[l][i] = rd16(q);
    }
    q = cap_section(f, "RBIO", &n);
    end = q + n;
    if (!q || n < 4) return fprintf(stderr, "%s: no RBIO section\n", what), -1;
    w->nbiomes = rd32(q);
    q += 4;
    if (w->nbiomes < 0 || w->nbiomes > RWG_MAX_BIOMES) return fprintf(stderr, "%s: %d biomes\n", what, w->nbiomes), -1;
    for (int i = 0; i < w->nbiomes; i++) {
        rwg_biome *b = &w->biome[i];
        if (q >= end) return fprintf(stderr, "%s: RBIO short\n", what), -1;
        b->present = *q++;
        if (!b->present) continue;
        if (q + 6 > end) return fprintf(stderr, "%s: RBIO short\n", what), -1;
        b->kind = q[0];
        b->base = rd16(q + 1);
        b->river = rd16(q + 3);
        b->nparams = q[5];
        q += 6;
        if (b->nparams > RWG_MAX_PARAMS || q + 4 * b->nparams > end) return fprintf(stderr, "%s: biome %d params\n", what, i), -1;
        for (int k = 0; k < b->nparams; k++, q += 4) b->param[k] = rdf32(q);
    }
    for (int l = 0; l < 6; l++)
        for (int i = 0; i < w->nlist[l]; i++)
            if (w->list[l][i] >= w->nbiomes || !w->biome[w->list[l][i]].present)
                return fprintf(stderr, "%s: list %d names biome %d\n", what, l, w->list[l][i]), -1;
    w->have = 1;
    return 0;
}
