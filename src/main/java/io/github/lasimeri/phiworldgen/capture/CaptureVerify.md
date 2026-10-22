# CaptureVerify.java

Slice S2's gate (`docs/plan.md`). It shows that a capture holds everything
the octaves and the combine depend on, using Minecraft's own classes, not a
port:

1. Build a new `ChunkProviderGenerate` from the captured seed XOR
   `0x5DEECE66D`. Its own state is then wrong in every octave.
2. Overwrite every octave's permutations and offsets, and
   `parabolicField`, with the captured ones.
3. For each `c_X_Z.bin`: set `biomesForGeneration` from `BI10` and call
   the real `func_147423_a(x * 4, 0, z * 4)` through the invoker.
4. Compare the four octave arrays and the field with the captured ones, bit
   for bit.

It reports how many chunks differ and the first difference: the stage, the
index and both values in hex.

The interpolation and the later stages have no inputs beyond what is
captured next to them, and the card port is checked against them directly
(`card/replay/replay.c`).
