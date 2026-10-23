# RWG capture format

What `/phiwg rwgcap <name> [biome radius]` writes to
`~/.cache/phiworldgen/capture/<name>/`
(`src/main/java/io/github/lasimeri/phiworldgen/capture/RwgCapture.java`),
what the card's replay (`card/replay/rwgreplay.c`) reads, and what the mod
sends pwgd as an RWG world state. RWG is Realistic World Gen alpha 1.5.2, the
world type GT New Horizons locks. The container is the one of
`capture-format.md`: a 4-byte magic, a `u32` version (1), then sections of a
4-byte tag, a `u32` length and the payload, all little-endian.

## `rwg-world.bin` (magic `RWGW`): once per capture, and the world state

| Tag | Payload |
| --- | --- |
| `SEED` | `i64` world seed |
| `NOIS` | RWG's `PerlinNoise` tables (the generator's and the biome manager's are made from one seed and are equal): `i32 p[8194]`, `f32 g1[8194]`, `f32 g2[16388]`, `f32 g3[24582]` |
| `CELL` | three `CellNoise`s, the generator's `cell`, the biome manager's `cell` and `biomecell`, each `i64` seed, `i64` seedOffset, `u8` useDistance, `i16` distanceMethod |
| `PARB` | `f32[289]`: `parabolicField` (17 x 17) |
| `LIST` | `u8` wetEnabled, `u8` smallEnabled; then the biome manager's six lists in the order snow, cold, hot, wet, small, test: `u32` n, n x `u16` realistic biome id. Order and repeats are kept: RWG picks `list.get((int)(h * n))` |
| `RBIO` | `u32` n; per realistic biome id below n: `u8` present; when present `u8` rNoise kind (`card/lib/rwg.h`: below 64 the terrain class, from 64 the biome's own code), `u16` base biome id, `u16` river biome id, `u8` k, `f32[k]` the terrain's parameters |

The mod refuses the card (`RwgCapture.refusal`) unless the generator is
`rwg.world.ChunkGeneratorRealistic`, its noise is the `NoiseGeneratorWrapper`
on Perlin (`useOpenSimplex` false), and every realistic biome's class is one
the card has.

## `h_X_Z.bin` (magic `PWGH`): one per chunk

What `getNewNoise(cmr, X * 16, Z * 16, biomes)` leaves; the capture calls the
generator's own method, then clears `mapGenBiomes[0..255]` as `provideChunk`
does.

| Tag | Payload |
| --- | --- |
| `CHNK` | `i32` x, `i32` z (chunk coordinates) |
| `HGTS` | `f32[256]`: the returned heights (`testHeight`), index `i * 16 + j` with i along x |
| `RBIO` | `i16[256]`: the realistic biome chosen per column, index `j * 16 + i` as RWG's `biomes[]`; -1 for none |
| `RIVR` | `f32[256]`: `riverStrength`, index `i * 16 + j` |
| `MGBI` | `f32[258]`: `mapGenBiomes`: the centre column's weight per realistic biome, then its ocean value and river strength |

The chunk set: 36 chunks around the origin, seven far out (past 2^24 blocks
the float coordinates lose their low bits), and the first chunk found for
each realistic biome within the radius (centres sampled every 4 chunks).

## pwgd's answer (flag 4)

The same values after the `u32` flags: `f32 height[256]`, `i16 biome[256]`,
`f32 river[256]`, `f32 mapgen[258]`, 3,596 bytes in all.
