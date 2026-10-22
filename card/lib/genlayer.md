# genlayer.c

The overworld's two GenLayer chains of 1.7.10 + Forge 10.13.4.1614, so the
card computes a chunk's biomes from its coordinates alone:

- `gl_slots10` / `gl_biomes10`: `getBiomesForGeneration` (10 x 10 from
  `cx * 4 - 2`), the biomes the shape stage blends;
- `gl_slots16` / `gl_biomes16`: `loadBlockGeneratorData` (16 x 16 from
  `cx * 16`), the biomes of the chunk's columns.

The `slots` forms return `getBiomeGenArray()` indexes, which the server maps
back to its own `BiomeGenBase` objects; the `biomes` forms return `biomeID`,
what the capture records. They differ: slot 161 holds a biome whose
`biomeID` is 160.

## Input

`world.bin`'s `LAYR` and `BIOX` sections (`capture/Capture.java`,
`docs/capture-format.md`):

- `LAYR`: the layer graph in post order, each layer's kind (19 kinds, the
  exact vanilla classes), mode, `baseSeed`, `worldGenSeed` as the server's
  layers hold them, and its inputs; `GenLayerBiome`'s four weighted lists as
  its constructor built them. Absent when a mod changed the chain.
- `BIOX`: per slot, whether a biome exists, its `getBiomeClass` (as an
  index), `getTempCategory` (OCEAN 0, COLD 1, MEDIUM 2, WARM 3), and flags:
  snow, `BiomeGenMesa`, Forge's ocean list, the jungle class.

Reading the seeds instead of recomputing them carries over a vanilla quirk:
`GenLayerHills` does not override `initWorldGenSeed`, so the two zoom layers
of its second input keep a world seed of 0.

## Arithmetic

GenLayer's LCG is 64-bit wrapping arithmetic (`uint64_t` here); `nextInt`
is `(chunkSeed >> 24) % n`, made non-negative. `GenLayerVoronoiZoom`'s
jitter is double arithmetic in Java's order, exact at the card's 53-bit x87
precision. Each `getInts` call takes its scratch arrays from a `gl_arena`
(1 << 18 ints a worker in pwgd).

## Verified

`card/replay` on captures from new seeds, every chunk's `BI10` and `BI16`
from coordinates against the server's: g1, g2, g3 (283 chunks, 44 to 55
biome ids each, mushroom islands and jungles included): 0 differ on the host
and on the 7120A. Live check mode on a new seed: 1,070 chunks, biomes 0
differ. Two bugs the gate found on the way: `TempCategory`'s MEDIUM is 2,
not 1; ids must go through the captured `biomeID`.
