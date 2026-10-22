/* genlayer.h: the overworld's GenLayer biome chains, from the capture's LAYR and BIOX sections. See genlayer.md. */
#ifndef GENLAYER_H
#define GENLAYER_H

#include <stdint.h>

#include "capfile.h"

/* The layer kinds, in the order of Capture.LAYER_KINDS. */
enum {
    GL_ISLAND,
    GL_FUZZYZOOM,
    GL_ZOOM,
    GL_ADDISLAND,
    GL_REMOVEOCEAN,
    GL_ADDSNOW,
    GL_EDGE,
    GL_ADDMUSHROOM,
    GL_DEEPOCEAN,
    GL_RIVERINIT,
    GL_BIOME,
    GL_BIOMEEDGE,
    GL_HILLS,
    GL_RIVER,
    GL_SMOOTH,
    GL_RAREBIOME,
    GL_SHORE,
    GL_RIVERMIX,
    GL_VORONOI,
    GL_KINDS
};

typedef struct {
    int32_t id, weight;
} gl_entry;

typedef struct {
    uint8_t kind, mode; /* mode: GenLayerEdge's, 0 COOL_WARM, 1 HEAT_ICE, 2 SPECIAL */
    int64_t base, wgs;  /* baseSeed, worldGenSeed as the server's layers hold them */
    int32_t in0, in1;   /* inputs (parent; GenLayerHills' river noise; GenLayerRiverMix's two chains), -1 none */
    /* GL_BIOME: the weighted lists of DESERT, WARM, COOL, ICY */
    uint8_t modded[4];
    int32_t n[4], total[4];
    gl_entry *list[4];
} gl_layer;

typedef struct {
    int have; /* 0: the capture has no LAYR (a mod changed the chain); the server sends the biomes */
    int n, root10, root16;
    gl_layer *l;
    /* BIOX, per biome id: exists, getBiomeClass index, temperature category, flags (1 snow, 2 BiomeGenMesa,
     * 4 Forge's ocean list, 8 the jungle class) */
    uint8_t present[256], cls[256], temp[256], flags[256];
    uint16_t bid[256]; /* getBiome(i).biomeID: not always i (slot 161 holds a biome whose id is 160) */
} gl_world;

/* Scratch for one evaluation: getInts' intermediate arrays come from here, reset per top-level call. */
typedef struct {
    int32_t *mem;
    int cap, used;
} gl_arena;

/* 0 with have set; 0 with have clear when LAYR is absent; -1 (and a message) on a malformed section. */
int gl_load(gl_world *g, const capfile *f, const char *path);
void gl_free(gl_world *g);

int gl_arena_init(gl_arena *a, int ints);
void gl_arena_free(gl_arena *a);

/* layer.getInts(x, z, w, h) into out (w * h ints). */
void gl_ints(const gl_world *g, gl_arena *a, int layer, int x, int z, int w, int h, int32_t *out);

/* The biome slots (getBiomeGenArray indexes, 0xFFFF for null) of getBiomesForGeneration for chunk cx, cz (10 x 10
 * from x = cx * 4 - 2) and of loadBlockGeneratorData (16 x 16 from x = cx * 16): what the server maps back to its
 * BiomeGenBase objects, and what BIOM is indexed by. */
void gl_slots10(const gl_world *g, gl_arena *a, int cx, int cz, uint16_t out[100]);
void gl_slots16(const gl_world *g, gl_arena *a, int cx, int cz, uint16_t out[256]);

/* getBiomesForGeneration for chunk cx, cz (10 x 10 from x = cx * 4 - 2) and loadBlockGeneratorData (16 x 16 from
 * x = cx * 16), as the biome ids the capture records: getBiome(id).biomeID, 0xFFFF for null. */
void gl_biomes10(const gl_world *g, gl_arena *a, int cx, int cz, uint16_t out[100]);
void gl_biomes16(const gl_world *g, gl_arena *a, int cx, int cz, uint16_t out[256]);

#endif
