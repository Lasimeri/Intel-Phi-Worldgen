# AccessorBiomeGenHills.java (and the surface accessors beside it)

What the surface capture (`Capture.surfaceTables`: sections `SURB`, `SNOI`,
`BLKS` of `world.bin`) reads, so that the card can run
`replaceBlocksForBiome` (`card/lib/surface.c`).

| Accessor | Field | What it is |
| --- | --- | --- |
| `AccessorBiomeGenHills` | `field_150636_aF`, `_150637_aG`, `_150638_aH` | the variant numbers: the surface turns to gravel where the stone noise is below -1 or above 2 when aH == aG, to stone above 1 when aH != aF |
| `AccessorBiomeGenTaiga` | `field_150644_aH` | the type: 1 and 2 (the mega taigas) rewrite their top block per column (podzol, coarse dirt) |
| `AccessorBiomeGenMutated` | `baseBiome` | a mutated biome's surface is its base's `genTerrainBlocks`, run on the base object |
| `AccessorNoiseGeneratorPerlin` | `field_151603_a` | the simplex octaves of `field_147430_m`, the stone noise |
| `AccessorNoiseGeneratorSimplex` | `field_151608_f` | an octave's permutations (its offsets are public) |

The Hills, Savanna M and mega Taiga overrides write `topBlock`, `fillerBlock`
and `field_150604_aj` on the biome object itself; the cave carver later reads
them. `SSTA` (per chunk) records those fields after the surface, so the
card's end state can be checked and set on the server's objects.
