/* shape.h: 1.7.10's terrain shape (func_147423_a and func_147424_a) in C. See shape.md. */
#ifndef SHAPE_H
#define SHAPE_H

#include <stdint.h>

#include "capfile.h"

/* One Perlin octave (NoiseGeneratorImproved). */
typedef struct {
    int32_t p[512];
    double x, y, z;
} octave;

/* NoiseGeneratorOctaves. */
typedef struct {
    int n;
    octave *o;
} octaves;

/* What the shape needs from world.bin. */
typedef struct {
    octaves gen[4]; /* noiseGen6, field_147429_l, field_147431_j, field_147432_k */
    float parabolic[25];
    float root[256], var[256];
    int amplified;
} world;

enum { BLOCK_STONE = 1, BLOCK_WATER = 9, BLOCK_NULL = 0xFFFF };

int world_load(world *w, const capfile *f, const char *path);

/* NoiseGeneratorOctaves.generateNoiseOctaves, the 3D form (ys == 1 takes the 2D branch, as in Java). */
void noise_octaves(const octaves *g, double *out, int x, int y, int z, int xs, int ys, int zs, double xsc,
                   double ysc, double zsc);

/* The four octave calls of func_147423_a for chunk cx, cz. */
void shape_octaves(const world *w, int cx, int cz, double o0[25], double o1[825], double o2[825], double o3[825]);

/* The rest of func_147423_a: the biome-weighted combine into the 5 x 33 x 5 field. */
void shape_combine(const world *w, const uint16_t bi10[100], const double o0[25], const double o1[825],
                   const double o2[825], const double o3[825], double field[825]);

/* The interpolation of func_147424_a: block ids (BLOCK_*), indexed x << 12 | z << 8 | y. */
void shape_interp(const double field[825], uint16_t blocks[65536]);

/* The whole shape of chunk cx, cz from its 10 x 10 GenLayer biome ids. */
void shape_chunk(const world *w, int cx, int cz, const uint16_t bi10[100], uint16_t blocks[65536]);

/* Run-length codes of a shape: per column (x outer, z inner, as the block index), runs from y = 0 up, each
 * two bytes: the kind (0 null, 1 stone, 2 water) and the run length minus one. Returns the bytes written;
 * out needs room for 131072 (the worst case, every block a run). */
size_t shape_rle(const uint16_t blocks[65536], uint8_t *out);

#endif
