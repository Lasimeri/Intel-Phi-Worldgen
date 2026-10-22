# MixinChunkProviderGenerate_Card.java

The live path of the terrain shape. At the head of `func_147424_a` it acts
only when four things hold:
- the card is enabled;
- no capture is running;
- the world is a `WorldServer`;
- the connected `CardClient` was given this very generator's state (`provider == this`). The card holds one world's generator; another `ChunkProviderGenerate`, such as `CaptureVerify`'s or a mod dimension's, gets vanilla.

It then calls `CardShape.fetch`:

- **With an answer**, it decodes it into the block array and cancels, so neither the biome lookup, nor the noise field, nor the interpolation runs. Skipping the method's first statement (`biomesForGeneration = getBiomesForGeneration(...)`) is safe: `provideChunk` passes that field straight to `loadBlockGeneratorData`, which overwrites it (or allocates it when null) before anything reads it again.
- **Without one**, it returns and vanilla runs as always.

In check mode it keeps the answer and lets vanilla run; at the return it
compares the two (`CardShape.compare`).

When it cancels it closes the profile's `SHAPE` phase, which the profile's
own return injection would otherwise have closed. The time spent here is
the `CARD` phase, prefetching included (`PREFETCH`).
