# rwgreplay.c

`rwgreplay DIR [threads] [rounds]` replays an RWG capture (`/phiwg rwgcap`,
`docs/rwg-capture-format.md`) through `lib/rwg.c`. It compares every
chunk's heights, chosen biomes, river strengths and `mapGenBiomes` with what
RWG made, bit for bit, and prints the chunks that differ, the counts, and
the time per chunk per thread. Threads 0 runs the self-test instead: hashes
of each piece (noise1, noise2, cell noise, cell border, biome choice, ocean,
river) on a grid, printed so the host's and the card's output can be
compared line for line. That is how the card's `floor()` was found.

Built by `build.sh` as `out/rwgreplay-host` (cc) and `out/rwgreplay-card`
(knc-cc, static). The card run sets the x87 precision control to 53 bits as
pwgd does. `~/.cache/phiworldgen/rwg-card-gate.sh THREADS ROUNDS` puts the
binary and every capture on the card under `/tmp/pwg/rwg` and replays them.

Measured 2026-10-23, three GT New Horizons captures (115 to 116 chunks each,
every realistic biome): 0 chunks differ on the host (8 threads) and on the
card (120 threads); 0.91 to 1.05 ms a chunk a thread on the host, 25.4 to
28.5 ms on the card.
