# pwgd.c

```
pwgd [-p PORT] [-w WORKERS] [-s SHM_OFFSET_MIB]      defaults 25600, 120, 1024
```

The card's terrain-shape daemon for the live server.

**Two transports:**
- **The shared host-memory region** (`docs/shm-protocol.md`): phictl pins it for the card through vfio. pwgd moves requests in and answers out with its own DMA channel (6, `card/lib/cdma.md`).
- **TCP** on `127.0.0.1:PORT`: reached through an `ssh -L` forward over phictl's port 2222, kept as the fallback. `-s 0` turns the shared region off.

**Threads:**
- **Workers:** each sets the x87 precision control to 53 bits, runs `shape_chunk` and `shape_rle` into its own 132 KiB slice of huge pages (15 slices per 2 MiB page, physically contiguous, so the engine can copy from it), then answers by its request's transport.
- **The shared-region dispatcher:** refuses the region when `/proc/swaps` lists `/dev/phiblk1` (the card would be swapping onto it). Otherwise it keeps a heartbeat, loads a new session's world state by DMA, pulls new requests by DMA and queues them (urgent first).
- **The TCP thread:** serves one connection at a time.

Each transport has a generation counter, so queued requests of an old
session or connection are dropped.

**Answers through the region:** a worker takes the next answer number n,
waits while n is 2048 ahead of what the host has taken, and submits one
copy into slot n % 2048. The copy's status descriptor writes n + 1 into
completion entry n % 2048 in host memory.

**Running it:**
- **The keeper:** `keeper.sh` (unit `phiwg-pwgd`) puts and starts it whenever the card answers and it is not running. `/tmp` on the card is tmpfs, so a card reboot removes it.
- **By hand:**

  ```
  phictl --card 0 exec -- /bin/mkdir -p /tmp/pwg
  phictl --card 0 put card/pwgd/out/pwgd-card /tmp/pwg/pwgd --mode 755
  phictl --card 0 exec -- /bin/sh -c 'cd /tmp/pwg && setsid nohup ./pwgd > /tmp/pwg/pwgd.log 2>&1 < /dev/null &'
  ```

**TCP protocol** (little-endian):

| Direction | What |
| --- | --- |
| hello | `PWGH`, `u32` 1, `u32` n, n bytes of `world.bin` |
| reply | `PWGR`, `u32` 1, `u32` workers |
| request | 212 bytes: `i32` x, `i32` z, `u32` flags (bit 0 urgent), `u16[100]` biome ids |
| answer | `i32` x, `i32` z, `u32` n, `u32` compute µs, n bytes of column runs |

**Measured** (2026-10-22, capture `s2a`, 455 chunks a run):

| Transport | Chunks/s | Differ | Notes |
| --- | --- | --- | --- |
| TCP through the tunnel | 1,156.5 | 0 | `pwgcheck` |
| shared region | 3,205.8 | 0 | `shmcheck`; session up in 4 ms, first answer after 17.2 ms |

Live over the shared region: 1,061 chunks compared with vanilla in check
mode, 0 differ.
