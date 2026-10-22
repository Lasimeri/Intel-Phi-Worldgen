# shmcheck.c, cdmatest.c

**`shmcheck CAPTURE_DIR [ROUNDS [OFFSET_MIB [REGION_FILE]]]`**, on the host,
drives a running pwgd through the shared host-memory region exactly as the
mod's `ShmLink` does:
1. Maps the part of `/dev/shm/phi-hostmem` and checks the card's heartbeat.
2. Opens a session with the capture's `world.bin`.
3. Posts every captured chunk ROUNDS times, a round at a time, so no more than 2048 are outstanding.
4. Takes the answers in completion order, decodes each one's column runs, and compares them with the capture's `SHAP`.

It prints the session time, chunks per second, the first answer's
latency, bytes per chunk and how many chunks differ.

**`cdmatest CHAN OFFSET_MIB`**, on the card, checks a DMA channel against
the region:
1. 64 KiB pulled in and compared.
2. 64 KiB pushed out, with a status word into host memory.
3. 64 MiB each way for bandwidth, as 128 copies of 512 KiB (one descriptor holds at most 16,383 lines).

The host can check the pushed pattern independently in `/dev/shm/phi-hostmem`.

Both are built by hand:

```
cc -O2 -std=gnu11 -I../lib -o out/shmcheck shmcheck.c ../lib/capfile.c ../lib/shape.c
bash -c '. ~/Intel\ Phi\ 7120A/toolchain/env.sh && knc-cc -O2 -std=gnu11 -I../lib -static -o out/cdmatest-card cdmatest.c ../lib/cdma.c -lpthread'
```

Results: `pwgd.md`, `../lib/cdma.md`.
