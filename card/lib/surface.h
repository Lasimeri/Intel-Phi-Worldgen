/* surface.h: replaceBlocksForBiome, the biome surface of 1.7.10's overworld, from the capture's SURB and SNOI
 * sections. See surface.md. */
#ifndef SURFACE_H
#define SURFACE_H

#include <stdint.h>

#include "capfile.h"

/* java.util.Random */
typedef struct {
    uint64_t s;
} jrand;
void jr_seed(jrand *r, int64_t seed);
int jr_next(jrand *r, int bits);
int jr_int(jrand *r, int bound);
double jr_double(jrand *r);
int64_t jr_long(jrand *r);
float jr_float(jrand *r);

/* NoiseGeneratorSimplex and NoiseGeneratorPerlin (its octaves) */
typedef struct {
    int32_t p[512];
    double x, y, z;
} simplex;
typedef struct {
    int n;
    simplex o[8];
} perlin;

/* The surface kinds of the biome classes (the genTerrainBlocks overrides) */
enum { SK_DEFAULT, SK_HILLS, SK_SAVANNA_M, SK_TAIGA, SK_UNPORTED, SK_MUTATED }; /* SK_UNPORTED: Mesa and mod biomes */

typedef struct {
    uint8_t present, kind, base; /* base: SK_MUTATED's baseBiome slot */
    uint8_t meta;                /* field_150604_aj & 255 */
    uint8_t hills;               /* SK_HILLS: 1 the gravel variant (aH == aG), 2 the stone variant (aH != aF) */
    uint8_t taiga;               /* SK_TAIGA: field_150644_aH */
    uint8_t top_air, filler_air; /* the block's material is air */
    uint8_t canon;               /* the first slot holding the same object */
    uint16_t top, filler;
    float temp;
} sbiome;

typedef struct {
    int have;
    perlin stone; /* ChunkProviderGenerate.field_147430_m */
    sbiome b[256];
    /* block ids (BLKS) */
    uint16_t air, stone_id, bedrock, gravel, sand, sandstone, water, ice, grass, dirt, lava;
} surf_world;

/* A mutating biome's fields after the chunk's last column of it: what the server sets on its object. */
typedef struct {
    uint8_t slot, meta;
    uint16_t top, filler;
} surf_state;

/* 0 with have set; 0 with have clear when SURB or SNOI is missing; -1 on a malformed section. */
int surf_load(surf_world *s, const capfile *f, const char *path);

/*
 * replaceBlocksForBiome on blocks (ids, 0xFFFF null; x << 12 | z << 8 | y) and metas, with the chunk's 16 x 16
 * biome slots (getBiomeGenArray indexes, gl_slots16's order). Returns the number of end states written to st (at
 * most 256), or -1 when a biome's surface is not ported (Mesa): blocks and metas are then untouched.
 */
int surf_chunk(const surf_world *s, int cx, int cz, const uint16_t slots16[256], uint16_t *blocks, uint8_t *metas,
               surf_state *st);

#endif
