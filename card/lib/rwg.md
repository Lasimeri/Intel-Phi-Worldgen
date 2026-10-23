# rwg.c

RWG alpha 1.5.2's height stage, `ChunkGeneratorRealistic.getNewNoise`, in C:
for a chunk, each column's height, the realistic biome RWG picks for it, its
river strength, and `mapGenBiomes`. The generator then fills, paints and
carves on the server as it would after its own call (`card/RwgCard.java`).

**Pieces, each operation for operation as RWG's bytecode:**
- `PerlinNoise.noise1` and `noise2`, on the tables from the world state (`NOIS`).
- `CellNoise.noise` and `border`, with `valueNoise2D`'s int arithmetic wrapping before the long.
- `ChunkManagerRealistic`: `getOceanValue`, `getBiomeDataAt`, `getRiverStrength`, `calculateRiver`.
  - `getBiomeDataAt(x, z)` computes an ocean value it never uses; the port skips it.
- `rNoise`: the 19 terrain classes and the 13 biomes with their own code.
  - The parameters of a biome's terrain come from the world state (`RBIO`).
- `getNewNoise`: the 21 x 21 biome samples, the parabolic weights, the `mix4` cascade, the per-column pick, and the weighted heights.

**Exactness.**
- Every float operation goes through `rf`, a store to a volatile float, as in `shape.c`. At the x87's 53-bit precision control, one float `+`, `-`, `*` or `/` then that store gives the IEEE single result.
- Doubles are exact at 53 bits.
- Casts to int follow Java: toward zero, NaN to 0, saturating.
- Two traps:
  - **The decompiled source drops parentheses** of right-nested `*` and `+`. Three expressions read differently in the bytecode: `TerrainGrasslandMountains`, `TerrainMesa` and the tropical island. Each such expression here was checked against `javap -c`.
  - **The card's musl `floor()`** (built for x87) adds 2^63 and needs the precision control at 64 bits; at 53 it is wrong. `jfloor` is the integer floor used instead. `sqrt` and `fabs` are exact.

**Measured** (`card/replay/rwgreplay.md`, 2026-10-23): three captures of GT New Horizons worlds, 347 chunks, every realistic biome:
- 0 chunks differ on the host and on the card.
- Cost: 0.91 to 1.05 ms a chunk a thread on the host (Ryzen 7 5800X), 25.4 to 28.5 ms on the card (one 7120A thread).
- Most of the card's time is `mix4` over all 256 ids and the `rf` stores.
