# Capture format

What `/phiwg capture <name>` writes to `~/.cache/phiworldgen/capture/<name>/`
(`src/main/java/io/github/lasimeri/phiworldgen/capture/Capture.java`), and
what the card's replay (`card/replay/`) reads. Everything is little-endian.

A file is a 4-byte magic, a `u32` version (1), then sections. A section is
a 4-byte ASCII tag, a `u32` payload length in bytes, and the payload.

## `world.bin` (magic `PWGW`): once per capture

| Tag | Payload |
| --- | --- |
| `SEED` | `i64` world seed; `u8` structures on; `u16` n; n bytes: the world type's name (`default`, `amplified`, ...) |
| `GENS` | `u32` generator count (4); for each, in the order `func_147423_a` calls them (`noiseGen6`, `field_147429_l`, `field_147431_j`, `field_147432_k`): `u32` octaves; for each octave: `i32[512]` permutations, `f64` xCoord, yCoord, zCoord |
| `PARB` | `f32[25]`: `parabolicField` |
| `BIOM` | `u32` count (256); for each biome id: `u8` present; `f32` rootHeight; `f32` heightVariation; `u16` top block id; `u16` filler block id; `u16` n; n bytes: the biome's class name |
| `SINT` | `f32[65536]`: `MathHelper`'s sine table |
| `BIOX` | `u32` count (256); per slot 8 bytes: `u8` present, `u8` `getBiomeClass` index, `u8` `getTempCategory` ordinal (OCEAN 0, COLD 1, MEDIUM 2, WARM 3), `u8` flags (1 snow, 2 `BiomeGenMesa`, 4 Forge's ocean list, 8 the jungle class), `i32` `biomeID` |
| `LAYR` | the GenLayer chains: `u32` layers, `i32` the 10 x 10 root, `i32` the 16 x 16 root; per layer in post order: `u8` kind (`card/lib/genlayer.h`), `u8` mode, `i64` baseSeed, `i64` worldGenSeed, `i32` input, `i32` second input; `GenLayerBiome` adds per BiomeType `u8` modded, `u32` n, n x (`i32` id, `i32` weight). Absent when a layer is not exactly a vanilla class |
| `SURB` | `u32` count (256); per slot 16 bytes: `u8` present, kind (`card/lib/surface.h`), mutated base slot, top metadata, Hills flags, Taiga type, air flags, first slot of the same object; `u16` top id, filler id; `f32` temperature |
| `SNOI` | the stone noise (`field_147430_m`): `u32` octaves; per octave `i32[512]` permutations, `f64` x, y, z |
| `BLKS` | `u16` ids: air, stone, bedrock, gravel, sand, sandstone, water, ice, grass, dirt, lava, flowing water, flowing lava |
| `CARV` | `i32` the cave carver's range, `i32` the ravine carver's |

## `c_X_Z.bin` (magic `PWGC`): one per chunk

In the order the generator produces them:

| Tag | Payload | Where it is taken |
| --- | --- | --- |
| `CHNK` | `i32` x, `i32` z (chunk coordinates) | |
| `BI10` | `u16[100]` biome ids | after `getBiomesForGeneration` in `func_147424_a` |
| `OCT0` | `f64[25]`: `noiseGen6` (2D, 5 x 5) | after the fourth octave call in `func_147423_a` |
| `OCT1` | `f64[825]`: `field_147429_l` | the same |
| `OCT2` | `f64[825]`: `field_147431_j` | the same |
| `OCT3` | `f64[825]`: `field_147432_k` | the same |
| `FLD ` | `f64[825]`: the combined field | after `func_147423_a` returns |
| `SHAP` | `u16[65536]` block ids | at the return of `func_147424_a` |
| `BI16` | `u16[256]` biome ids | the array `replaceBlocksForBiome` received |
| `SURF` | `u16[65536]` block ids | at the return of `replaceBlocksForBiome` |
| `META` | `u8[65536]` block metadata | the same |
| `CAVE` | `u16[65536]` block ids | at the return of the cave generator's `func_151539_a` |
| `RAVN` | `u16[65536]` block ids | at the return of the ravine generator's |
| `FLAG` | `u32`: bit 0 = a Forge `ReplaceBiomeBlocks` handler denied the surface | |
| `SSTA` | `u32` n; n x (`u16` slot, `u16` top id, `u16` filler id, `u8` top metadata, `u8` 0): the fields of every biome object whose surface override rewrites them | at the return of `replaceBlocksForBiome` |

The noise arrays are indexed `(x * 5 + z) * 33 + y` (3D) and `x * 5 + z`
(2D), the order `populateNoiseArray` fills them in. Block arrays are indexed
`x << 12 | z << 8 | y`, as in `func_147424_a`. A block id is
`Block.getIdFromBlock`; `0xFFFF` is a `null` entry (the generator's air), so
it stays distinct from `Blocks.air` (id 0). A biome id is `biomeID`;
`0xFFFF` is `null`.
