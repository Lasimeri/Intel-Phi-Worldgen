# Intel Phi Worldgen

A Forge mod for Minecraft 1.7.10 that moves overworld generation off the
server thread onto an Intel Xeon Phi 7120A coprocessor, bit for bit as
vanilla generates it. The card runs `pwgd`, a C daemon whose noise octaves
run on the card's 512-bit vector unit.

## What runs on the card

| Stage | On the card | Verified |
| --- | --- | --- |
| Biomes (both GenLayer chains, 10 x 10 and 16 x 16) | live, scalar | replay 283 chunks 0 differ; live check 1,070 chunks 0 differ |
| Terrain shape: 3D noise octaves | live, **vector unit** | replay 7 captures 0 differ; about 1.15 M VPU instructions a chunk (hardware counter) |
| Terrain shape: 2D octaves, combine, interpolation | live, scalar | live check 0 differ |
| Surface (`replaceBlocksForBiome`) | ported, live-gated (protocol 3), not yet deployed on the play server | replay 267 chunks 0 differ (Mesa not ported); live check 1,029 whole chunks 0 differ |
| Caves, ravines (`MapGenCaves`, `MapGenRavine`, Forge's `digBlock`) | ported, live-gated (protocol 3), not yet deployed | replay 267 chunks 0 differ; whole chunks from coordinates 261 of 261 equal; live check 1,029 whole chunks 0 differ |
| Decorations (`populate`) | stay on the CPU | |

The server asks ahead (a prefetch ring around each chunk it generates) and
never waits more than 1 ms; any chunk the card has not answered is made by
vanilla on the CPU. Check mode (`/phiwg card check on`) runs both and
compares every block and biome.

Measured, card off against on, 5 interleaved pairs, each pair a new random
seed, 1,089 chunks each (`docs/plan.md`): chunks per second up 12.5 to
18.8 % (median 16.3 %), the server thread's `provideChunk` time down 18.0 to
27.4 %.

## Layout

- `src/main/java/...`: the mod (mixins into `ChunkProviderGenerate`, the
  capture, the card client, `/phiwg`).
- `card/lib/`: the C ports (`shape`, `vshape` on the vector unit, `carve`,
  `genlayer`, `surface`) and the card's DMA engine (`cdma`).
- `card/pwgd/`: the card daemon and its host-side checkers.
- `card/replay/`: replays a capture's stages and compares every bit.
- `card/tools/vpucount.c`: counts VPU instructions with the card's
  performance counters.
- `docs/`: the plan, the capture format, the shared-region protocol.

Every code file has a `.md` beside it.

## Build

The mod: JDK 25, `./gradlew build` (`runServer` for the JDK 8 dev server).
The card side needs the Intel Phi 7120A stack (`knc-cc`, `phictl`) and the
Intel-Phi-LLVM S3 clang for the vector kernels: `card/pwgd/build.sh`,
`card/replay/build.sh`.

## Running it

**The dev server (how it runs here).** `./gradlew runServer` compiles the
source, builds a development jar (`build/libs/phiworldgen-<commit>-dev.jar`)
and starts Forge 1.7.10 with it loaded. Nothing goes into the server's `mods/`
folder, and nothing is downloaded from this repository: GitHub holds the
source only (`build/` is not in git). The server's log lists the mod as
loaded: `LoadedMods [... phiworldgen ...]`.

**A regular Forge 1.7.10 server.** `./gradlew build` makes the release jar
in `build/libs/` (not yet built and tested as a standalone install). Put it
in the server's `mods/` folder together with a Mixin loader such as
UniMixins: Forge 1.7.10 has no Mixin of its own (the dev run gets one through
the template's dependencies).

**The card side** runs apart from any jar: `pwgd` on the Xeon Phi, started and
kept running by `card/pwgd/keeper.sh`, and phictl's shared host-memory region
(`/dev/shm/phi-hostmem`), which the mod maps. Without the card the mod falls
back to vanilla generation on the CPU, chunk by chunk.

**Clients** need nothing: the mod is server-side only. A plain Forge 1.7.10
client with an empty `mods/` folder joins and plays; the client never
generates terrain in multiplayer (its chunk provider only fills chunks from
the server's packets).
