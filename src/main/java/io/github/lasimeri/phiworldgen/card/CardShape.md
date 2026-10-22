# CardShape.java

The server thread's side of the card's terrain shape (slice S5,
`docs/plan.md`). The server thread never waits on the card beyond
`waitMs`.

**`fetch(world, x, z)`**, called from `MixinChunkProviderGenerate_Card`:

1. **Prefetch:** asks the card for the chunks in rings of 1 to `radius` around x, z that are not loaded, not on disk and not already asked for. It goes nearest ring first and stops after `perChunk` requests, so one chunk never pays for a whole ring.
   - Disk is checked by the region file first, then `AnvilChunkLoader.chunkExists`, because that call creates a missing region file.
   - The 10 x 10 GenLayer biomes of each prefetched chunk are computed here, on the server thread: GenLayer's `IntCache` is not thread-safe.
2. **Take** the chunk's own entry:
   - answered: the runs are returned (`ready`, or `waited` when it came within `waitMs`);
   - never asked (`misses`) or not answered within `waitMs` (`late`): null, and the CPU makes the chunk at vanilla's cost. A late answer is dropped.

**The other calls:**
- **`decode`**: writes the runs into the block array exactly as `func_147424_a` would: `Blocks.stone`, `Blocks.water` or `null`.
- **`compare`** (check mode): decodes into a second array and compares each entry with vanilla's by identity. It counts a differing chunk and logs the first ten.
- **`status`** (`/phiwg card status`): the connection and answers, card shapes used, CPU shapes (misses, late), prefetches, failures, and the check counts.

**Measured** (2026-10-22, plain Forge on JDK 8, fresh test worlds, centre
chunk 67,16, `phiwg pregen 16` = 1,089 chunks; logs
`~/.cache/phiworldgen/s5-*.log`):

| Run | Result |
| --- | --- |
| check mode, twice | 1,056 chunks compared, 0 differ; 0 waits; 33 made by the CPU (1 never asked, 32 late) |
| card off / on, five interleaved pairs, chunks/s | 286.9 / 296.8, 269.2 / 298.2, 272.7 / 286.0, 270.8 / 281.1, 274.2 over 271.3 (pairs 4 and 5 with `perChunk` 16) |
| server-thread cost of the card path a chunk | 0.200 to 0.277 ms, of which prefetch 0.166 to 0.244, against vanilla's shape at 0.542 to 0.608 |
| worst chunk in the card phase | 10.3 to 11.3 ms without the cap, 5.56 ms with it (one capped run had 55.2 ms, during a JVM pause that also hit `populate` at 107.7 ms) |
