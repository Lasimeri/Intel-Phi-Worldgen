# AccessorGenLayer.java (and the GenLayer accessors beside it)

What the layer capture (`Capture.layers`, section `LAYR` of `world.bin`)
reads from the overworld's biome layers, so that the card can compute the
10 x 10 and 16 x 16 biome ids of a chunk from its coordinates alone.

| Accessor | Field | What it is |
| --- | --- | --- |
| `AccessorGenLayer` | `parent`, `baseSeed`, `worldGenSeed` | every layer's input and its two LCG seeds, read after `initWorldGenSeed` (so the capture records the seeds the layers really use) |
| `AccessorGenLayerRiverMix` | `biomePatternGeneratorChain`, `riverPatternGeneratorChain` | the two inputs of the last 10 x 10 layer |
| `AccessorGenLayerHills` | `field_151628_d` | the second input of `GenLayerHills`: the river-init noise zoomed twice |
| `AccessorGenLayerEdge` | `field_151627_c` | the mode: `COOL_WARM`, `HEAT_ICE` or `SPECIAL` |
| `AccessorGenLayerBiome` | `biomes` (Forge, not remapped) | the four weighted lists (`DESERT`, `WARM`, `COOL`, `ICY`) as the constructor built them, the world type's desert extras included |
| `AccessorWorldChunkManager` | `genBiomes`, `biomeIndexLayer` | the roots of the two chains |

`GenLayerHills` does not override `initWorldGenSeed`, so the two zoom layers
of its second input never get a world seed (they keep 0); reading the seeds
instead of recomputing them carries that over.
