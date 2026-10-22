/* carve.h: MapGenCaves and MapGenRavine (with Forge's digBlock) on a chunk's block array. See carve.md. */
#ifndef CARVE_H
#define CARVE_H

#include <stdint.h>

#include "capfile.h"
#include "surface.h"

typedef struct {
    int have;
    int64_t seed;
    int range_caves, range_ravines;
    float sin_table[65536]; /* MathHelper.SIN_TABLE */
    uint16_t stone, grass, dirt, water, flowing_water, lava, flowing_lava;
} carve_world;

/* What digBlock and isTopBlock ask of a column's biome, x + z * 16: its top and filler blocks as they stand after
 * the surface (Blocks.grass and Blocks.dirt for the exception biomes: mushroom island, beach, desert). */
typedef struct {
    uint16_t top[256], filler[256];
} carve_cols;

/* 0 with have set; 0 with have clear when CARV, SEED, SINT or the extra BLKS ids are missing; -1 on error. */
int carve_load(carve_world *c, const capfile *f, const char *path);

/* The columns for a chunk from its 16 x 16 slots, the surface tables, and the end states of its surface. */
void carve_columns(const surf_world *s, const uint16_t slots16[256], const surf_state *st, int nst, carve_cols *out);

/* caveGenerator.func_151539_a and ravineGenerator.func_151539_a on blocks (x << 12 | z << 8 | y, 0xFFFF null). */
void carve_caves(const carve_world *c, const carve_cols *cols, int cx, int cz, uint16_t *blocks);
void carve_ravines(const carve_world *c, const carve_cols *cols, int cx, int cz, uint16_t *blocks);

#endif
