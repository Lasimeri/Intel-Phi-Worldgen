/* rwg.h: RWG alpha 1.5.2's height stage (ChunkGeneratorRealistic.getNewNoise) in C, operation for operation as the
 * decompiled Java: its Perlin and cell noise, ChunkManagerRealistic's biome choice and rivers, every terrain's
 * generateNoise and the inline rNoise of the coast, island and test biomes. See rwg.md. */
#ifndef RWG_H
#define RWG_H

#include <stdint.h>

#include "capfile.h"

/* rNoise kinds: below 64 the terrain class a biome delegates to, from 64 the biome's own code
 * (capture/RwgCapture.java TERRAINS and INLINE, same order) */
enum {
    RT_BASE, RT_CANYON, RT_DUNES, RT_DUNEVALLEY, RT_FLATLAKES, RT_GRASSFLATS, RT_GRASSHILLS, RT_GRASSMOUNTAINS,
    RT_HIGHLAND, RT_HILLY, RT_MARSH, RT_MESA, RT_MOUNTAIN, RT_MOUNTAINRIVER, RT_MOUNTAINSPIKES, RT_POLAR,
    RT_SMALLSUPPORT, RT_SWAMPMOUNTAIN, RT_SWAMPRIVER,
    RI_BASE = 64, RI_TEST, RI_TESTRIVER, RI_OCEANTEST, RI_COASTCOLDCLIFF, RI_COASTCOLDSLOPE, RI_COASTDUNES,
    RI_COASTICE, RI_COASTMANGROVE, RI_COASTOASIS, RI_COASTTEST, RI_ISLANDTROPICAL, RI_ISLANDTUNDRA
};

#define RWG_MAX_BIOMES 256
#define RWG_MAX_PARAMS 16

typedef struct {
    int p[8194];
    float g1[8194], g2[16388], g3[24582];
} rwg_perlin;

typedef struct {
    int64_t seed, offset;
    int use_distance, method;
} rwg_cell;

typedef struct {
    int present, kind, base, river, nparams;
    float param[RWG_MAX_PARAMS];
} rwg_biome;

typedef struct {
    int have;
    int64_t seed;
    rwg_perlin perlin;           /* the generator's and the biome manager's: one seed, one table set */
    rwg_cell cell, cmr_cell, biomecell;
    float parabolic[289];
    int wet_enabled, small_enabled;
    int nlist[6];                /* snow, cold, hot, wet, small, test */
    uint16_t list[6][256];
    int nbiomes;
    rwg_biome biome[RWG_MAX_BIOMES];
} rwg_world;

/* What getNewNoise leaves for the chunk at block x, z (the chunk's corner): heights indexed i * 16 + j (i along x),
 * the chosen realistic biome per column indexed j * 16 + i (as RWG's biomes[]), river strength i * 16 + j, and
 * mapGenBiomes: the centre column's weights per realistic biome, then its ocean value and river strength. */
typedef struct {
    float height[256];
    int16_t biome[256];
    float river[256];
    float mapgen[258];
} rwg_heights;

/* Loads the sections of rwg-world.bin (docs/rwg-capture-format.md); 0, or -1 with a message. */
int rwg_load(rwg_world *w, const capfile *f, const char *what);

/* Scratch for one thread: the 9 x 9 and 25 x 25 biome weight grids (about 720 KB). */
typedef struct {
    int biome_data[441];
    float huge[81][256];
    float small[625][256];
} rwg_scratch;

void rwg_new_noise(const rwg_world *w, rwg_scratch *s, int x, int z, rwg_heights *out);

/* The pieces, for tests. */
float rwg_noise1(const rwg_perlin *p, float x);
float rwg_noise2(const rwg_perlin *p, float x, float y);
float rwg_cell_noise(const rwg_cell *c, double x, double z, double frequency);
float rwg_cell_border(const rwg_cell *c, double x, double z, double width, float depth);
int rwg_biome_at(const rwg_world *w, int x, int z);
float rwg_ocean(const rwg_world *w, int x, int z);
float rwg_river_strength(const rwg_world *w, int x, int z);

#endif
