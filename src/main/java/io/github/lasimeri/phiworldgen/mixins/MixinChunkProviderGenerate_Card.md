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

## Whole chunks (protocol 3)

The card is now asked at the head of `provideChunk`. A whole answer makes the
chunk right there: the card's blocks and metadata, its 16 x 16 biomes, the
surface's end states set on the biome objects, then vanilla's tail (the four
structure generators when map features are on, `new Chunk`, the biome array,
`generateSkylightMap`), and the method returns it. A shape answer goes on as
before (`func_147424_a`, then the biome redirect). In check mode vanilla makes
everything and the card's whole chunk is compared with the returned `Chunk`
(null and air-material blocks both read back as air, as `new Chunk` stores
them) and its end states with the objects' fields.

## The chunk without a block array

When the four structure generators are `MapGenStructure`s (whose
`func_151538_a` is final and never reads the block array; vanilla passes it
null elsewhere), a whole answer is turned into the `Chunk` directly
(`CardShape.buildChunk`): the card's runs go straight into each section's
id and metadata arrays, with the non-air and random-tick counts that
`func_150818_a` keeps. No 65,536-entry `Block[]` is made or walked. Check
mode compares those sections with vanilla's, array for array and count for
count (side server, new seed: 1,027 chunks, 0 differ).
