# MixinChunkProviderGenerate_Profile.java

Injections into 1.7.10's overworld generator, `ChunkProviderGenerate`, that
mark where each phase starts and ends for `GenProfile` (the phase table is
in `GenProfile.md`). They only record time; the generator runs unchanged.
The boundaries are the decompiled source's
(`build/rfg/minecraft-src/java/net/minecraft/world/gen/ChunkProviderGenerate.java`
after `setupDecompWorkspace`):

- `provideChunk`: HEAD and RETURN; around the calls to
  `loadBlockGeneratorData`, `replaceBlocksForBiome`, the two
  `MapGenBase.func_151539_a` calls (ordinal 0 the caves, 1 the ravines: the
  four structure generators are called through their own classes, so they
  do not match this owner), the `NEW net/minecraft/world/chunk/Chunk`, and
  `generateSkylightMap`.
- `func_147424_a` (the terrain shape): HEAD and RETURN, around
  `getBiomesForGeneration`, and after the call to `func_147423_a`.
- `func_147423_a` (the noise field): HEAD, the `PUTFIELD field_147425_f`
  that stores the fourth octave array (`noiseGen6`, `field_147429_l`,
  `field_147431_j`, `field_147432_k` are called in that order), and RETURN.
- `populate`: HEAD and RETURN.

The names are MCP's (stable 12); `func_147423_a` and `func_147424_a` have no
MCP name in 1.7.10. The refmap the annotation processor writes maps them to
the obfuscated names for a release jar.
