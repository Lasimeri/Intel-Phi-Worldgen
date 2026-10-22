# Plan

Overworld terrain for Minecraft 1.7.10 (Forge 10.13.4.1614) computed on
the Intel Xeon Phi 7120A, so the server's CPU time goes to mods instead.
Started 2026-10-22. The vector-unit plan for everything after the shape stage is `docs/vpu-plan.md`. The scope was widened the same day, after the
measurements below, from the octave noise alone to the whole terrain:
shape, caves, ravines, then the surface. The comparison is plain Forge
1.7.10 generation: C2ME, the modern generation accelerator, has no 1.7.10
version.

## Why 1.7.10

The user asked for the popular modded version that is easiest to do this
for. 1.7.10's overworld (`ChunkProviderGenerate`) is a fixed algorithm:
octave Perlin noise on a 5 x 5 x 33 grid per chunk, a biome-weighted
combine, then trilinear interpolation to blocks. Minecraft 1.18 and later
build terrain from a data-driven density-function graph, which would need a
graph compiler on the card before the first chunk. The build setup is
GTNewHorizons/ExampleMod1.7.10 at 6453dec (RetroFuturaGradle, Gradle 9.7.1,
JDK 25 toolchain, Java 8 bytecode through Jabel).

What it does not cover: GT New Horizons locks its world type to RWG
(`config/defaultworldgenerator.cfg`: `World Generator=RWG`, `Lock World
Generator=true`), a different generator. This mod does nothing in a GTNH
world until RWG itself is ported. Tests run in the RetroFuturaGradle dev
environment with the default world type.

## Where generation runs in 1.7.10

Confirmed in the decompiled source: a new chunk is generated synchronously
on the server thread. `ChunkProviderServer.loadChunk` sends only chunks
already on disk to the asynchronous loader ("We can only use the queue for
already generated chunks"); a new one goes through `originalLoadChunk` to
the generator's `provideChunk` on the calling thread. So the offload
relieves the tick thread directly, and the card's round trip sits on that
thread's critical path. The live path (S5) therefore computes chunks ahead
of the player, and `provideChunk` only collects them.

## What the card computes

In the order `provideChunk` runs them:

| Stage | Share (S1, plain Forge) | On the card | Inputs from the CPU | Output |
| --- | --- | --- | --- | --- |
| octaves | 10.93 to 10.96 % | the four `generateNoiseOctaves` calls; vector units from S4 | the generators' permutation tables and offsets, once per world | 3 x 825 + 25 doubles |
| combine | about 0.2 % | the biome-weighted combine (float weights, float division: on x87 each float operation computed in double and rounded to float, which is exact for one operation) | the 10 x 10 GenLayer biome ids; each biome's root height and variation, once per world | the 5 x 33 x 5 field |
| interpolation | 3.01 to 3.12 % | trilinear interpolation to stone, water or air | | 65,536 block ids |
| surface | 8.34 to 8.51 % | `replaceBlocksForBiome` for the vanilla biomes (`genBiomeTerrain` and the overrides in Hills, Mesa, Taiga, Ocean, Savanna, Swamp, Mutated); a chunk with any other biome stays on the CPU | the surface Perlin generator's state, once | block ids and metadata |
| caves | 9.68 to 9.87 % | `MapGenCaves` for the 17 x 17 source chunks | the block array after the surface; the 16 x 16 biome ids; the JVM's `MathHelper` sine table, once | block ids |
| ravines | 2.20 to 2.22 % | `MapGenRavine` | as caves | block ids |

Together these are about 34.6 % of the generator's server-thread time, a
ceiling of 1.53 times the chunks per second (arithmetic). The GenLayer biome
lookups (7.2 to 7.6 %) stay on the CPU for now. So do structures, `new Chunk`
(8.5 to 8.7 %), the skylight and `populate` (45 %): they build Java objects
or reach into the world.

## Slices and their gates

| Slice | Work | Gate |
| --- | --- | --- |
| S0 | JDK 8, 17 and 25; `setupDecompWorkspace`, `build` | done 2026-10-22: the mod loads in the dev server |
| S1 | Profile a fixed-seed pregeneration in the dev server, phase by phase | done 2026-10-22, below |
| S2 | A capture mixin writes the live generators' state once per world, and each chunk's inputs and outputs at every stage boundary in the table above, for N chunks | the files read back into the same arrays the game produced |
| S3 | Scalar ports on the card (`knc-cc -static`, x87 at 53-bit precision), each replaying the capture, in pipeline order: shape (octaves, combine, interpolation), then the surface, then caves and ravines (they read the surface's blocks), so a chunk makes one round trip and one block-array conversion | 0 bits different from the capture, stage by stage |
| S4 | The octaves on the vector units (MVEX), across columns and chunks | 0 bits different; time per chunk |
| S5 | The live path: chunks computed ahead of the player (and in order during pregeneration), collected by `provideChunk`; the CPU path when the card is late or absent | worlds identical to vanilla, block for block, over a radius. Shape stage live 2026-10-22 (`card/pwgd/`, `CardShape`, non-blocking): check mode 1,056 chunks compared, 0 differ (`s5-gate3.log`); card off / on interleaved, 5 pairs, faster in each: 269.2 to 286.9 / 274.2 to 298.2 chunks/s |
| S6 | Speed | chunks per second and server-thread time per chunk against S1, interleaved repeats |

## S1 result (2026-10-22)

### The reference: plain Forge on Java 8

`runServer` starts Forge 1.7.10 with this mod alone, on JDK 1.8.0_504 (no
Hodgepodge: 0 mentions in the logs). This is the vanilla behaviour the card
must reproduce, and the baseline for speed:

```
TASK=runServer TASK_ARGS= bash ~/.cache/phiworldgen/s1-repeat.sh v1 v2 v3
```

Three runs, fresh world each, seed 20261022, `phiwg pregen 16` (1,089
chunks), Ryzen 7 5800X; logs `~/.cache/phiworldgen/s1-v{1,2,3}.log`:
286.9, 282.3 and 276.4 chunks/s.

| Phase | Share of `provideChunk` + `populate` | ms a chunk |
| --- | --- | --- |
| GenLayer biomes, 10 x 10 | 2.42 to 2.49 % | 0.075 to 0.080 |
| octaves | 10.93 to 10.96 % | 0.342 to 0.353 |
| interpolation | 3.01 to 3.12 % | 0.097 |
| GenLayer biomes, 16 x 16 | 4.79 to 5.07 % | 0.151 to 0.161 |
| surface | 8.34 to 8.51 % | 0.264 to 0.272 |
| caves | 9.68 to 9.87 % | 0.302 to 0.319 |
| ravines | 2.20 to 2.22 % | 0.069 to 0.071 |
| structures | 1.40 to 1.44 % | 0.044 to 0.046 |
| `new Chunk` | 8.50 to 8.71 % | 0.272 to 0.275 |
| `generateSkylightMap` | 0.82 to 0.84 % | 0.026 |
| `populate` | 45.40 to 45.77 % | 1.519 to 1.563 |

The card's stages (octaves, combine, interpolation, surface, caves,
ravines) come to about 34.6 %: a ceiling of about 1.53 times the chunks per
second (arithmetic).

### With Hodgepodge, on Java 25

`runServer25` adds lwjgl3ify, GTNHLib and Hodgepodge 2.7.206, as GT New
Horizons runs. Hodgepodge patches the generator:

- `fastload.rand` replaces `java.util.Random` in `ChunkProviderGenerate`,
  `MapGenBase` and `MapGenCaves` with its `StdLCG`. It is the same LCG, but
  its `setSeed` does not apply Java's seed scramble.
- It defers population and throttles chunk generation.

So the numbers below are not vanilla's, and parity is checked against
`runServer` only. They were measured on OpenJDK 25.0.5:

```
JAVA_HOME=/usr/lib/jvm/java-25-openjdk ./gradlew --no-configuration-cache runServer25 -x setupHotswapAgent25
> phiwg pregen 16          (1,089 chunks, centred 64 chunks east of spawn)
```

| Run | Chunks/s | Octaves, ms a chunk | Octaves share | `populate` share |
| --- | --- | --- | --- | --- |
| 1 | 325.1 | 0.365 | 13.56 % | 43.32 % |
| 2 | 316.0 | 0.373 | 13.46 % | 42.16 % |
| 3 | 338.4 | 0.351 | 13.58 % | 42.40 % |

Shares are of `provideChunk` plus `populate`, all on the server thread.
The rest:

| Phase | Share |
| --- | --- |
| carvers and structures | 13.36 to 13.89 % |
| Chunk and skylight | 10.78 to 11.08 % |
| surface | 6.75 to 6.93 % |
| shape | 20.61 to 20.66 % (the noise field plus the 10 x 10 GenLayer biomes and the interpolation) |

The four octave calls are 13.5 % of vanilla 1.7.10's generation time. With
them at zero, chunks per second would rise at most 1.156 to 1.157 times
(arithmetic, not measured). A modded pack adds to `populate`, so the share
there is smaller. That ceiling is why the scope was widened.

The same runs with the profile split further (logs
`~/.cache/phiworldgen/s1-b{1,2,3}.log`): 332.5, 321.0 and 317.8 chunks/s.

| Phase | Share | ms a chunk |
| --- | --- | --- |
| GenLayer biomes, 10 x 10 | 3.05 to 3.14 % | 0.080 to 0.086 |
| noise field | 13.77 to 13.97 % | 0.362 to 0.381 |
| of which octaves | 13.51 to 13.72 % | 0.355 to 0.374 |
| interpolation | 3.75 to 3.79 % | 0.100 to 0.104 |
| GenLayer biomes, 16 x 16 | 4.38 to 4.52 % | 0.115 to 0.123 |
| surface | 6.85 to 6.91 % | 0.181 to 0.190 |
| caves | 8.06 to 8.16 % | 0.212 to 0.225 |
| ravines | 3.68 to 3.91 % | 0.101 to 0.108 |
| structures | 1.53 to 1.59 % | 0.040 to 0.044 |
| `new Chunk` | 9.81 to 9.90 % | 0.260 to 0.273 |
| `generateSkylightMap` | 1.04 to 1.05 % | 0.028 to 0.029 |
| `populate` | 42.19 to 42.94 % | 1.202 to 1.242 |

## Parity

Worlds must be bit-identical to vanilla: same seed, same world, and no seam
where a card chunk meets a CPU chunk. Java's double arithmetic is IEEE with
no fused multiply-add, so the card computes multiplies and adds separately.
The scalar port sets the x87 precision control to 53 bits and reads it back
(measured on the card: it starts at 0x037F, 64-bit precision, where
`(1+2^-30)(1-2^-30) - 1` gives -2^-60 instead of Java's 0; after `fldcw
0x027F` it gives 0). Float operations are computed in double and rounded to
float one operation at a time, which gives the IEEE single result for +, -,
*, / and square root. `MathHelper.floor_double`, `floor_double_long` and the
`% 16777216L` are ported as Java writes them. The sine table is the JVM's
own, uploaded, not recomputed. 1.7.10's 3D noise reuses four intermediate
values computed at the first height of each vertical noise cell for the
rest of that cell; the port reproduces that. A generator replaced through
Forge's `InitNoiseGensEvent` stays on the CPU.

## Boundaries

- Its own card program and its own transport. Slices 1 to 4 move files with
  `phictl put` and `phictl exec`.
- No change to `phi-vpu-worker` or to the Intel Phi 7120A tree without the
  user's say. A new `knc-mvex-gen` subcommand would be such a change.
- No decompiled Minecraft source in this repository.
