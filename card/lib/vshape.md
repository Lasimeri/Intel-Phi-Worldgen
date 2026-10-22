# vshape.c

The three 3D octave generators of `ChunkProviderGenerate.func_147423_a`
(`field_147429_l`, `field_147431_j`, `field_147432_k`, 8 + 16 + 16 octaves)
on the 7120A's 512-bit vector unit, bit for bit as the scalar port
(`shape.c`).

- Lanes are columns: 25 columns a chunk as two groups of 16 lanes, each
  group two vectors of 8 doubles. The height loop and the shared-height
  cache's recompute decision are the same for every column, so they stay
  scalar and uniform.
- Floor is `vrndfxpntpd` rounding toward minus infinity; the permutation
  lookups are 16-lane `vpgatherdd` (`kncgather.S`); the gradients come from
  16-entry float tables through `vpermd` and an exact `vcvtps2pd`.
- No fused multiply-add: every multiply and add stays separate, in Java's
  order.

Built with the Intel-Phi-LLVM S3 clang (`-march=knc`), linked into the
x87 card programs with knc-cc.

## Verified

- Replay on the 7120A, every value against the capture: s2a (91 chunks),
  g1, g2, g3, s1, s2, s3: 0 differ.
- VPU_INSTRUCTIONS_EXECUTED (`card/tools/vpucount`): about 1.15 M a chunk;
  the scalar path executes 0.
- Replay timing on the card (verification-side, one thread): 15.07 ms a
  chunk scalar, 5.04 ms vector.
