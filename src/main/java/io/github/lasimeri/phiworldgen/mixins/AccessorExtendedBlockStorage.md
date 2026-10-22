# AccessorExtendedBlockStorage.java

`blockRefCount` and `tickRefCount` of a chunk section: the number of non-air
blocks and of randomly ticking ones. `new Chunk(world, blocks, metas, x, z)`
keeps them through `func_150818_a`; `CardShape.buildChunk` writes the section
arrays directly and sets the same counts, and check mode compares them.
