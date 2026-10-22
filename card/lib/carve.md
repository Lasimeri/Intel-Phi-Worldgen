# carve.c

The cave and ravine carvers of 1.7.10 + Forge 10.13.4.1614 on a chunk's
block array, after the surface: `MapGenCaves` and `MapGenRavine` through
`MapGenBase.func_151539_a`, with Forge's `digBlock` and `isTopBlock`.

- **Sources.** As vanilla: the base `Random` seeded with the world seed
  gives two longs; every source chunk within `range` (8) is re-seeded with
  `x * l ^ z * i1 ^ seed` and walks its tunnels (rooms, branches, the early
  exit), carving only inside the target chunk.
- **Arithmetic.** The tunnels' angles and widths are Java floats: on the
  card's x87 every float operation is rounded through a volatile store
  (`rf`), which gives the IEEE single result at 53-bit precision
  (53 >= 2 * 24 + 2). `MathHelper.sin` and `cos` read the captured
  `SIN_TABLE`.
- **digBlock.** The biome of a column is the 16 x 16 biome at `x + z * 16`
  (the chunk is not loaded yet, so `getBiomeGenForCoords` reads the biome
  cache). Its top and filler blocks are as the surface left them: the
  surface tables, with the end states of the objects the surface overrides
  rewrote (`carve_columns`). Mushroom island, beach and desert use grass and
  dirt. Caves leave lava below y 10, ravines flowing lava.

## Input

`world.bin`'s `SEED`, `SINT`, `BLKS` (with flowing water and flowing lava)
and `CARV` (the two ranges), plus `surface.h`'s tables.

## Verified

`card/replay` on captures from new seeds c1, c2, c3 (267 chunks; 32,086 to
42,479 blocks carved by caves and 5,658 to 29,864 by ravines a capture):
caves from the captured surface, ravines from the captured caves, and the
whole chunk from coordinates (biomes, shape, surface, caves, ravines, 261
chunks; Mesa skipped) against the server's blocks and metadata: 0 differ on
the host and on the 7120A.
