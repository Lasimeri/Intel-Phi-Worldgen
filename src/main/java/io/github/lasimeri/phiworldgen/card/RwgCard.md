# RwgCard.java

The card's answer for an RWG world (pwgd flag 4: heights, column biomes,
river strengths, `mapGenBiomes`), turned into what `generateTerrain` leaves:
- the generator's `testHeight`, `riverStrength` and `mapGenBiomes`;
- the `biomes[]` RWG's own pick would give;
- the block array: stone up to `(int)` height, water below 63, air above.

The rest of `provideChunk` runs on the CPU on that state, as after RWG's own
call: the per-biome map gens, `replaceBlocksForBiome`, the caves,
mineshafts, strongholds and villages, `new Chunk`, and populate. That covers
GregTech's ore veins and every other mod's generators.

`compare` is check mode. It builds the answer's arrays next to the ones
`generateTerrain` just made on the CPU and compares them bit for bit: blocks,
biomes, heights, rivers and `mapGenBiomes`. It logs the first ten chunks that
differ.

This class names RWG's types, so it loads only with RWG present, from the
late mixin.
