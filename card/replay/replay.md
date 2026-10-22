# replay.c

```
replay DIR [THREADS [ROUNDS]]
```

Replays a capture directory (`docs/capture-format.md`) through the C ports
and compares every bit, then times them. Built for the host and the card by
`build.sh`; on the card:

```
phictl --card 0 exec -- /tmp/pwg/replay /tmp/pwg/s2a 122 100
```

**Precision control.** It first sets the x87 precision control to 53 bits
and prints the control word before and after (the card starts at `0x037f`,
64-bit precision). The timing threads set it again, since each thread
starts with its own.

**Stage checks.** For every `c_X_Z.bin`, each stage runs from the captured
inputs of that stage, so a difference names the stage that made it:

1. the octaves from the chunk coordinates, against `OCT0` to `OCT3`;
2. the combine from the captured octaves and `BI10`, against `FLD`;
3. the interpolation from the captured `FLD`, against `SHAP`;
4. the whole shape from the coordinates and `BI10`, against `SHAP`.

For each stage it prints how many chunks differ and the first difference,
with the index and both values in hex. It exits 1 when any stage differs.

**Timing.** The whole shape for every chunk, ROUNDS times, on THREADS
threads (chunk i goes to thread i mod THREADS). It prints chunks per second
and the time per chunk per thread.
