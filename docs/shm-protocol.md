# The shared-region transport between the mod and pwgd

The fast path between the server's JVM and the card. It runs through the
host RAM that phictl shares with the card:
- on the host, the file `/dev/shm/phi-hostmem`, which phictl maps, populates and pins for the card through vfio (`VFIO_IOMMU_MAP_DMA` at IOVA 4 GiB);
- on the card, the same memory at card address `0x81_0000_0000`, reachable through `/dev/phihost` (uncached) and by the card's DMA engine.

pwgd takes DMA channel 6 for itself (`card/lib/cdma.md`). The mod maps the
same file with `FileChannel.map`, so neither side copies through anything
but the engine.

Everything sits in one part of the 2 GiB region, from **1 GiB** (default;
the mod's `shmOffsetMiB`, pwgd's `-s`). That is above the 768 MiB the
AVX-512 wrapper's worker uses from offset 0, and below 2 GiB, where the card
puts its swap when phictl gives it 4 GiB or more. With `--host-mem 2G`
the card swaps onto the whole region when that worker is not running, so
pwgd refuses this transport whenever `/proc/swaps` lists `/dev/phiblk1`.

All values are little-endian. Offsets are from the part's start:

| Offset | Size | Written by | What |
| --- | --- | --- | --- |
| 0 | 4 | host | magic `PWGS` |
| 4 | 4 | host | version, 3 |
| 8 | 8 | host | session: raised for each new world state |
| 16 | 4 | host | the world state's length |
| 64 | 8 | host | posted: requests written so far this session |
| 128 | 8 | host | completed: answers taken so far this session (their slots are free again) |
| 192 | 8 | card | the session the card has loaded |
| 200 | 4 | card | its worker threads |
| 256 | 8 | card | consumed: requests the card has taken from the ring |
| 320 | 8 | card | heartbeat, counted up about every 100 ms |
| 64 KiB | up to 1,966,080 | host | the world state (`world.bin`, `docs/capture-format.md`) |
| 2 MiB | 4096 x 256 | host | the request ring: entry n % 4096 holds request n: `i32` x, `i32` z, `u32` flags (bit 0 urgent, bit 1 the card computes the biomes, bit 2 the server takes the whole chunk), `u16[100]` the 10 x 10 biome ids (zero with bit 1) |
| 3 MiB | 2048 x 64 | card's DMA engine | the completion ring: entry n % 2048 holds `u64` n + 1 once answer n is in its slot |
| 4 MiB | 2048 x 135,168 | card's DMA engine | the answer slots: slot n % 2048 holds a 64-byte header (`i32` x, `i32` z, `u32` length, `u32` compute microseconds, `u64` n + 1) and the payload: `u32` flags (bit 0: the 16 x 16 biomes follow; bit 1: the whole chunk), `u16[256]` the 16 x 16 biome slots (`getBiomeGenArray` indexes), then either the whole chunk (below) or the shape's column runs (`card/lib/shape.h`) |

## A session

**The host:**
1. Zeroes the completion ring.
2. Sets posted and completed to 0.
3. Writes the world state, its length, the version and the magic.
4. After a store fence, raises the session.
5. Waits for the card to echo it (192).

**The card:**
1. Sees the new session.
2. Pulls the world state with the DMA engine and loads it.
3. Resets consumed and its answer counter.
4. Writes its worker count, then the session.

## Requests

The host writes entries, then, after a store fence, posted. It never
writes more than 4096 past consumed.

The card's dispatcher thread:
1. Reads posted through its uncached map, about every 50 µs while idle.
2. Pulls the new entries into card memory with the engine.
3. Queues them, urgent ones first.
4. Writes consumed.

## Answers

A worker:
1. Writes its column runs and the 64-byte header into its own slice of a huge page.
2. Takes answer number n, waiting while n - completed reaches 2048.
3. Submits one descriptor line:
   - the copy into slot n % 2048;
   - a status descriptor that writes n + 1 into completion entry n % 2048, in host memory, so its arrival implies the copy's;
   - a status descriptor into card memory for its own completion.

The host's reader thread:
1. Waits for completion entry `completed % 2048` to read `completed + 1`.
2. After a load fence, reads the slot.
3. Hands the answer to the waiting request.
4. Advances completed.

## Version 2 (2026-10-22)

Version 2 moved the GenLayer biomes onto the card. When `world.bin` holds
the `LAYR` section, the mod sends coordinates only (request flag bit 1) and
pwgd computes the 10 x 10 biomes itself (`card/lib/genlayer.c`); every
answer then also carries the chunk's 16 x 16 biome slots, which the mod
hands to `provideChunk` in place of `loadBlockGeneratorData`. The TCP link
carries the same requests and payloads, with version 2 in its hello.
Version 1 peers are refused.

## Version 3 (2026-10-22)

Version 3 lets the card make the whole chunk: biomes, shape, surface, caves
and ravines (`card/lib/surface.c`, `card/lib/carve.c`). The mod asks for it
(request flag bit 2) when the world state holds `LAYR`, `SURB`, `SNOI` and
`CARV` and no mod listens to Forge's `ReplaceBiomeBlocks`. A whole answer
(payload flag bit 1) carries, after the biome slots:

- `u16` n, then n end states of 8 bytes (`u16` biome slot, `u16` top block,
  `u16` filler block, `u8` top metadata, `u8` 0): the fields the surface
  overrides (Hills, Savanna M, mega Taiga) left on those biome objects, which
  the mod sets on its own;
- per column, `x << 4 | z`, from y 0: runs of `u16` block id (0xFFFF null),
  `u8` metadata, `u8` length - 1.

A chunk with a biome whose surface is not ported (Mesa), or whose runs would
pass the slot, is answered with its shape alone, and the server makes the
rest. Measured over captures c1, c2, c3: 7,008 to 7,532 bytes a whole chunk.

## pwgd's stats block (2026-10-22)

pwgd writes, with every heartbeat (about every 100 ms), 128 bytes at offset
384 of its part, which phitop (Intel Phi 7120A tree, `host/asm/phitop`) shows
as its `pwgd` line:

| Offset | What |
| --- | --- |
| 384 | `u64` answers since pwgd started |
| 392 | `u64` DMA errors |
| 400 | `u64` the mean worker's busy nanoseconds (compute over workers) |
| 408 | `u64` compute nanoseconds, all workers |
| 416 | `u64` 0 |
| 424 | `u64` DMA nanoseconds of the answers |
| 432 | `u32` 0, `u32` workers |
| 440 | `u64` the heartbeat count |
| 448 | `u32` magic `PWGD`, `u32` version 1 |
| 456 | `u64` VPU instructions (VPU_INSTRUCTIONS_EXECUTED, user mode) |
| 464 | `u64` whole answers |
| 472 | `u64` shape-only answers |

The first 64 bytes follow the AVX-512 wrapper worker's `struct vpu_stats`.
Each worker counts its own VPU instructions (KNC event 0x2016 on itself) and
adds them after every chunk. Only one such event counts on a 7120A thread, so
nothing else may count pwgd's threads while it runs (`card/tools/vpucount`
is for other programs; the benchmark reads offset 456 instead).
