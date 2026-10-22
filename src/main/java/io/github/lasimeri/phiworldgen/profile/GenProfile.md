# GenProfile.java

Slice S1's measurement (`docs/plan.md`): the wall time of each phase of
overworld chunk generation, summed over every chunk since the last reset.
The phases are marked by `MixinChunkProviderGenerate_Profile`, which calls
`begin` and `end` around them:

| Phase | From, to |
| --- | --- |
| `provideChunk` | the whole of `ChunkProviderGenerate.provideChunk` |
| shape | `func_147424_a`: everything below down to "interpolation" |
| biomes 10 x 10 | `getBiomesForGeneration`, the GenLayer biomes at 4-block resolution around the chunk |
| noise field | `func_147423_a`: the four `generateNoiseOctaves` calls and the biome-weighted combine |
| octaves | from the start of `func_147423_a` to the store of its fourth noise array (`field_147425_f`) |
| interpolation | from the return of `func_147423_a` to the end of `func_147424_a`: trilinear interpolation of the 5 x 33 x 5 field into the block array |
| biomes 16 x 16 | `loadBlockGeneratorData`, the per-block biomes (cached) |
| surface | the call to `replaceBlocksForBiome` |
| caves | the first `MapGenBase.func_151539_a` call (`caveGenerator`) |
| ravines | the second (`ravineGenerator`) |
| structures | from the ravines' return to the `new Chunk`: mineshafts, villages, strongholds, scattered features (when structures are on) |
| new Chunk | from `new Chunk` to the call of `generateSkylightMap` (the block array to sections, biome ids) |
| `generateSkylightMap` | that call |
| `populate` | the whole of `ChunkProviderGenerate.populate` (lakes, dungeons, biome decoration, animals, ice and snow) |

Nested phases are included in their parents, so the shares do not add up
to 100 %. The share column is against `provideChunk` plus `populate`, which
is the generator's whole cost on the server thread.

The counters are plain fields with no locking: 1.7.10 generates on the
server thread (`ChunkProviderServer.loadChunk`: only chunks already on disk
take the asynchronous queue). The report lists every thread that entered
`provideChunk` or `populate`, so a second thread would show.
