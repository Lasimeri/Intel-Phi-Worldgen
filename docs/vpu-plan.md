# Plan: as much of generation as possible on the card's vector unit

Written 2026-10-22 on the desktop, where this mod and the 7120A run, after
the shape stage went live on the card (scalar x87, `card/pwgd/`). This
plan covers what else can leave the server thread, and which of it runs
on the 512-bit vector unit (VPU) rather than the card's scalar x87.

Two different things are gained:

- **Moving a stage to the card at all** takes its time off the Minecraft
  server thread. That is what raises chunks per second, and it is true
  whether the card runs the stage scalar or vector.
- **The VPU** makes the card's work per chunk shorter. That buys headroom:
  prefetch keeps ahead at view distance 20 and in fast flight, and the card
  has capacity for more stages.

## Where things stand (measured)

| Fact | Number | Source |
| --- | --- | --- |
| Plain Forge 1.7.10, JDK 8, card off | 269.2 to 286.9 chunks/s | `docs/plan.md`, `~/.cache/phiworldgen/s5-ab-off*.log` |
| Card shape live, non-blocking | 274.2 to 298.2 chunks/s, faster in 5 of 5 interleaved pairs | `s5-ab-on*.log` |
| Live check, card against vanilla | 1,056 chunks, 0 differ | `s5-gate3.log` |
| Server-thread cost of the card path | 0.200 to 0.277 ms a chunk, of which 0.166 to 0.244 is the prefetch's biome lookups | the same logs |
| Card, scalar shape, one thread | 15.09 ms a chunk (16.6 to 18.4 ms live) | `card/lib/shape.md`, `pwgd.log` |
| Card, scalar shape, 122 threads | 3,906 to 4,011 chunks/s | `card/lib/shape.md` |
| Transport (ssh tunnel over phictl's forward) | 1,156.5 chunks/s at 1,138 B a chunk | `card/pwgd/pwgd.md` |

The rest of the generator, share of the server thread's generation time
(plain Forge, `docs/plan.md`):

| Phase | Share |
| --- | --- |
| GenLayer biomes 10 x 10 | 2.42 to 2.49 % |
| GenLayer biomes 16 x 16 | 4.79 to 5.07 % |
| surface | 8.34 to 8.51 % |
| caves | 9.68 to 9.87 % |
| ravines | 2.20 to 2.22 % |
| structures | 1.40 to 1.44 % |
| `new Chunk` | 8.50 to 8.71 % |
| skylight | 0.82 to 0.84 % |
| `populate` | 45.40 to 45.77 % |

## What the vector unit can and cannot do

From `~/Intel Phi AVX-512 C/docs/research/avx512-on-knc.md`, which cites
Intel's ISA reference 327364-001, and the measurements in the AVX-4096
vault note:

- **Width:** 512 bits: 8 doubles, or 16 floats or dwords, per instruction; 16-bit write masks on every operation; one vector instruction a cycle per core. The core is in order, so it needs two or more threads per core to issue every cycle.
- **FMA:** present, but Java never fuses, so for bit-exact results every multiply and add stays separate (`vmulpd`, `vaddpd`).
- **No divide or square root,** in float or double, and no double-precision reciprocal. A correctly rounded divide is a Newton sequence plus a residual fix, about 12 to 16 instructions in double (estimate in that doc). Stages that divide stay scalar or keep the divide on the CPU side.
- **Integer arithmetic is 32-bit only** (`vpaddd`, `vpadcd`, `vpaddsetcd`, `vpmulld`, `vpmulhud`). Java's 48-bit `Random` and GenLayer's 64-bit LCG need multiword arithmetic: a 64-bit multiply is three 32-bit partial products and carries.
- **Gathers** complete only the lanes that share a cache line per issue and must loop until the mask clears. Small tables (a 2 KiB permutation table) stay in L1.
- **Denormals:** flushed to zero (DAZ/FTZ always on). None of the values here are near the denormal range; the gates check that.
- **Floor and convert:** `vrndfxpntpd` and the conversions round in a chosen mode, which covers Java's `(int)` truncation plus the floor correction.

## Stage by stage

| Stage | On the VPU? | How | Parity hazards | Output |
| --- | --- | --- | --- | --- |
| octaves (10.9 %, on the card now, scalar) | yes | 8 columns per vector, drawn from a batch of chunks (25 columns a chunk; 8 chunks = 25 full vectors) | as the scalar port: no FMA, Java's floor, the shared-height cache | the 4 noise arrays (in card memory) |
| combine (0.2 %) | no | stays scalar on the card: float divisions, 25 columns a chunk | already exact (`card/lib/shape.c`) | the field |
| interpolation (3.1 %, on the card now, scalar) | yes | lanes are the 8 columns of a noise cell; the per-height accumulation stays in order inside each lane; a compare gives a mask, the mask gives block codes | the order of the additions (kept per lane) | block codes |
| GenLayer biomes, 10 x 10 and 16 x 16 (7.2 %, plus the prefetch's 0.17 to 0.24 ms a chunk) | yes | each layer is a function of its parent's neighbourhood, so lanes are output cells. The LCG of each cell (`initChunkSeed`, `nextInt`) is 64-bit arithmetic in 32-bit pieces. About 20 layer kinds (`GenLayer.initializeAllBiomeGenerators`) | Forge's biome weights per temperature group are world inputs to capture; a world type or mod that adds layers stays on the CPU | biome ids |
| surface (8.4 %) | partly | RNG part on the VPU (detail below); the per-column block logic is scalar or masked across columns once the start states are known; temperature noise on the VPU | sandstone draws and `nextInt`'s rejection loop change the call count: detect and redo | blocks, metadata |
| caves (9.8 %), ravines (2.2 %) | carving, yes | walks once per source chunk (scalar), then vector carving (detail below) | the early exit, rooms, the water check, the first-top-block flag | blocks |
| `new Chunk` (8.6 %), skylight (0.8 %) | yes | the card packs the section arrays and the height and sky light maps; a mixin builds the `Chunk` from arrays instead of 65,536 `func_150818_a` calls | the reference counts, the random-tick count, the empty-section rule | sections, maps |
| structures (1.4 %) | no | they build Java objects and record structure starts, which `populate` uses; cheap | | |
| `populate` (45 %) | measure first | it reads and writes the world across 2 x 2 chunks, its random sequence depends on what is already there (dungeons look for air), and mods hook into it through Forge events | | |

### Surface in detail

The decompiled `BiomeGenBase.genBiomeTerrain` makes one `nextDouble`, then
one `nextInt(5)` at every height from 255 down to 0 (the bedrock test
`l1 <= 0 + rand.nextInt(5)`). It also makes an extra `nextInt(4)` when
sand turns to sandstone. All 256 columns share one `Random`, so a chunk is
a single chain of about 65,800 LCG steps.

On the VPU:
- **Within a column:** the jump-ahead form, state(n+k) = A(k) x state(n) + C(k) mod 2^48, gives 8 consecutive draws per vector.
- **Across columns:** each column's start state depends on how many draws the columns before it made. The plan computes them speculatively, assuming no sandstone and no rejection, checks the true counts, and recomputes from the first column whose count was wrong.

Biomes with their own surface code (Hills, Mesa with its bands, Taiga,
Ocean, Savanna M, Swamp, the Mutated ones) are ported one by one. A chunk
holding any other biome is made by the CPU.

### Caves and ravines in detail

Vanilla re-simulates every tunnel of the 17 x 17 source chunks around each
chunk it generates, and carves only the part inside that chunk. Each walk's
random draws and path do not depend on the target chunk, with two
exceptions:

- **The early exit** (`d8*d8 + d9*d9 - d10*d10 > d11*d11`) returns only when the remaining steps cannot reach the target. It never removes a segment that would carve there, and the branch tunnels it would have skipped are smaller, so they cannot reach either.
- **A room** (`flag2`) moves step by step until its first step inside the target's box, carves there, and stops. Where it carves depends on the target, so rooms are carved per target exactly as vanilla does.

So the card simulates each source chunk's tunnels once, scalar, and caches
the steps in card memory (16 GB). For each target it carves on the VPU:

- lanes are 8 heights of a column, tested against the ellipsoid (`d13² + d12² + d14² < 1`, `d12 > -0.7`);
- the water check over the box decides whether to carve;
- the top, filler and lava rules follow, with a per-column "top block found" flag carried down as a prefix;
- sine and cosine come from the JVM's table (captured, gathered); the float math runs in 32-bit lanes, which round as IEEE single.

This is an algorithmic saving (one walk per source chunk instead of up to
289) on top of the vector one.

## What can be gained (arithmetic, not measured)

Every stage above except structures and `populate` comes to about 51 % of
the server thread's generation time. With all of it at zero, chunks per
second rise at most 1 / (1 - 0.51) = 2.05 times. The real figure is lower
by whatever the card path still costs on the server thread: today 0.20 to
0.28 ms a chunk, mostly prefetch biomes, which V3 removes. `populate` (45 %)
caps everything at about 2.2 times unless part of it moves too.

## What has to be built first

1. **A vector compiler on the desktop.** Intel-Phi-LLVM at e673241 (S3: KNC code generation and `kncintrin.h`) has only ever been built on the rack. `scripts/build.sh all` builds the clang variant (X86 target only, clang and lld, Release) into `~/.cache/intel-phi-llvm-build/llvm`. Time and disk on this machine are not measured; LLVM X86-only Release builds are typically about an hour on 16 threads and 10 to 20 GB (estimate). `/` has 60 GB free. The other way, new `knc-mvex-gen` subcommands, is a change to the Intel Phi 7120A tree.
2. **A faster transport, before any stage that returns whole arrays.** The ssh tunnel moves about 1.3 MB/s of shape runs. Final blocks with metadata, and packed sections, are tens to about 100 KB a chunk. The window phictl shares between host and card (`/dev/shm/phi-hostmem` on the host, `/dev/phihost` on the card) is the fast path:
   - card writes to host memory are posted (vector stores make 256 B transactions);
   - card reads are about 1 µs each, so requests are pulled in blocks;
   - Java maps the file region with `FileChannel.map`.

   It is the same window the AVX-512 wrapper's worker uses (its first 768 MiB). With `--host-mem 2G` the whole region becomes card swap when that worker is not running. The way out is `--host-mem 4G` (swap from 2 GiB, as on the rack) and a worldgen region at [1 GiB, 1.5 GiB). That touches the shared stack.
3. **The capture extended** for each new stage's inputs and outputs:
   - Forge's biome weights;
   - the surface noise (`NoiseGeneratorSimplex` state) and the temperature noise;
   - section and sky light state;
   - a `populate` profile split into its parts.

   s2a also has no mushroom island or jungle chunk; a wider biome search fixes that.
4. **pwgd as a stage pipeline:**
   - biomes, shape, surface, caves, ravines, then packing;
   - a scheduler that groups chunks for full vector lanes;
   - 2 to 4 threads per core.

   It should also survive a card reboot: today it lives in the card's tmpfs.

## Order and gates

Every stage keeps the rule so far: replay of a capture on the card with 0
bits different, then live check mode with 0 chunks different, then card off
and on interleaved for speed.

| Step | Work | Gate |
| --- | --- | --- |
| V0 | the vector compiler built on the desktop; a vector kernel run on the 7120A | Intel-Phi-LLVM's `s3-gate.sh` on the 7120A: its kernels bit-identical to the `.byte` originals |
| V1 | octaves on the VPU | replay of s2a: 0 bits different from the scalar port and the capture; card chunks/s against 3,906 to 4,011 scalar |
| V2 | interpolation on the VPU, then the whole shape on the VPU live | as V1, then live check 0 differ |
| V3 | GenLayer 10 x 10 and 16 x 16 on the card; prefetch sends coordinates only | captured biome ids from coordinates alone, 0 different, on every biome id found; the server-thread card cost against 0.20 to 0.28 ms |
| V4 | the window transport | MB/s and latency measured; live parity |
| V5 | surface on the card (VPU RNG, scalar column logic, per-biome ports) | SURF and META 0 different, on every vanilla biome with its own surface code |
| V6 | caves and ravines: walk cache plus vector carving | CAVE and RAVN 0 different, including water aborts, rooms and negative coordinates |
| V7 | sections and sky light packed on the card, the `Chunk` built from arrays | check mode compares every section and map with vanilla's `Chunk` |
| V8 | `populate` profiled part by part | a decision, with the numbers |
| V9 | the whole card path against plain Forge | chunks/s and server-thread time a chunk (median and 99th percentile), 5 interleaved pairs |

## Decisions that are yours

1. **The compiler:** build Intel-Phi-LLVM's vector clang on the desktop (recommended: it keeps the kernels in C in this repository and leaves the 7120A tree alone), or add `knc-mvex-gen` subcommands to the 7120A tree.
2. **The transport:** use the host-memory window, with `--host-mem 4G` and a worldgen region beside the AVX-512 worker's. Without it, the stages after V3 stay limited by the tunnel.
3. **pwgd's home:** the card's persistent `/data` disk, or the boot image (a 7120A tree change), so it survives a card reboot.
4. **`populate`:** stays on the CPU unless V8 finds a separable part.
