# surface.c

`ChunkProviderGenerate.replaceBlocksForBiome` of 1.7.10 + Forge 10.13.4.1614
on a chunk's block array: the stone noise, then each column's
`genTerrainBlocks`.

- `java.util.Random` (JDK 8) as `jrand`: `setSeed`, `next`, `nextInt` with
  its rejection loop, `nextDouble`, `nextLong`, `nextFloat`.
- The stone noise: `field_147430_m`, a 4-octave `NoiseGeneratorPerlin` of
  `NoiseGeneratorSimplex` octaves, `func_151599_a(x * 16, z * 16, 16, 16,
  0.0625, 0.0625, 1.0)`.
- `genBiomeTerrain` exactly, including vanilla's index `(z * 16 + x) * 256 +
  y`, which swaps x and z against the shape's array.
- The overrides: `BiomeGenHills` (gravel or stone by the noise and its
  variant numbers), `BiomeGenSavanna.Mutated`, `BiomeGenTaiga` types 1 and 2
  (coarse dirt, podzol), `BiomeGenMutated` (its base's surface, run on the
  base object). `BiomeGenSwamp`'s extra loop is dead code (it stops at the
  first block from y 255, which is null) and runs as the default.
  `BiomeGenMesa` and biomes of other mods are not ported: `surf_chunk`
  returns -1 and the server makes that chunk's surface.

## The end state

The Hills, Savanna M and mega Taiga overrides write `topBlock`,
`fillerBlock` and `field_150604_aj` on the biome object itself, and Forge's
cave carver reads them afterwards. `surf_chunk` returns each rewritten
object's fields after the chunk's last column of it (`surf_state`, by the
first slot that holds the object): the carvers use them, and the server sets
them on its objects when it takes the card's surface.

## Input

`world.bin`'s `SURB` (per slot: kind, mutated base, top metadata, Hills
variant flags, Taiga type, air flags, the first slot of the same object, top
and filler ids, temperature), `SNOI` (the stone noise's permutations and
offsets) and `BLKS` (block ids).

## Verified

`card/replay` on captures from new seeds s1, s2, s3 (267 chunks): `SURF`
block ids, `META` and 30 end states against the server's: 0 differ on the
host and on the 7120A; 2 Mesa chunks skipped.
