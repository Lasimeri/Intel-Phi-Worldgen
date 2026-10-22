# Capture.java

Writes a capture (`docs/capture-format.md`): the generator's state once,
then each chunk's values at every stage boundary, which the capture mixins
hand it while `on()`.

- **`start`** first refuses anything that is not exactly vanilla. That means the provider class, the four octave generators' classes (after Forge's `getModdedNoiseGenerators`), the surface noise, and the cave and ravine generators (after `getModdedMapGen`). A replaced class would make the capture describe some other generator. It then writes `world.bin`: the seed, the structures flag and the world type; every octave's permutations and offsets; `parabolicField`; each biome's height, variation, top and filler blocks and class; and `MathHelper`'s sine table.
- **`beginChunk` and `endChunk`** bracket one `provideChunk`. The buffer is 1 MiB; a chunk takes 617,268 bytes. `endChunk` writes `c_X_Z.bin` and returns how many blocks the caves and the ravines changed: `blocks` counts the differences between `SURF` and `CAVE`, and between `CAVE` and `RAVN`.
- **Block ids** come from `Block.getIdFromBlock`; a `null` entry is written as `0xFFFF`, so it stays distinct from `Blocks.air` (id 0).
