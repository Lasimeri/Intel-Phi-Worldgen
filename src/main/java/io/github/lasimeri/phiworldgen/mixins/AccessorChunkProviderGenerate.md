# AccessorChunkProviderGenerate.java

The private parts of 1.7.10's overworld generator that the capture writes
out and the verification drives. Field names are MCP's (stable 12); several
have no MCP name, so here is what each one is, from the decompiled
`func_147423_a` and constructor:

| Accessor | Field | What it is |
| --- | --- | --- |
| `getMinLimit` | `field_147431_j` | 16 octaves, 684.412 on every axis: the lower bound of the density |
| `getMaxLimit` | `field_147432_k` | 16 octaves, 684.412: the upper bound |
| `getMain` | `field_147429_l` | 8 octaves, 8.555150000000001 / 4.277575000000001 / 8.555150000000001: the blend between the bounds |
| (public) | `noiseGen6` | 16 octaves, 2D, 200 x 200: the depth noise |
| `getSurfaceNoise` | `field_147430_m` | the 4-octave `NoiseGeneratorPerlin` behind `replaceBlocksForBiome`'s stone noise |
| `getDepthNoise`, `getMainNoise`, `getMinLimitNoise`, `getMaxLimitNoise` | `field_147426_g`, `_147427_d`, `_147428_e`, `_147425_f` | the four octave outputs of the last chunk (25, 825, 825, 825 doubles) |
| `getField` | `field_147434_q` | the 5 x 33 x 5 field after the combine |
| `getParabolicField` | `parabolicField` | the 25 biome-blend weights, `10 / sqrt(j*j + k*k + 0.2)` in float |
| `getBiomes`, `setBiomes` | `biomesForGeneration` | the 10 x 10 GenLayer biomes of the last `func_147424_a` (and later the 16 x 16 of `loadBlockGeneratorData`, same array) |
| `getWorldType` | `field_147435_p` | the world type; `AMPLIFIED` changes the combine |
| `getCaves`, `getRavines` | `caveGenerator`, `ravineGenerator` | after Forge's `getModdedMapGen`, which may have replaced them |
| `noiseField` | `func_147423_a(x, y, z)` | the noise field: the four octave calls and the combine |
