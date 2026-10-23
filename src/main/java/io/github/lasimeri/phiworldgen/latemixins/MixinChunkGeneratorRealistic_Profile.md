# MixinChunkGeneratorRealistic_Profile.java

GenProfile's phases for RWG's `provideChunk` (`func_73154_d`) and populate
(`func_73153_a`): `generateTerrain` (and its `getNewNoise`), the per-biome
map gens, `replaceBlocksForBiome`, the caves, the structures, `new Chunk`,
`generateSkylightMap`. Pseudo, by class name, in SRG names with remap off.

Measured with it (GT New Horizons client, radius-16 pregen, warm JIT,
2026-10-23): provideChunk 1.689 ms a chunk, of which heights 0.527, the
fill 0.132, surface 0.091, caves 0.420, `new Chunk` 0.347, skylight 0.122;
populate 2.003 ms.
