# Census.java

What else takes part in overworld generation in this game, logged at server
start. That covers four things:
- the generator class and world type;
- for RWG, its caves generator and that generator's Random, plus RWG's biome lists and every realistic biome (class, base, river, beach);
- the listeners of Forge's terrain hooks (ReplaceBiomeBlocks, InitNoiseField, PopulateChunk, InitMapGen, InitNoiseGens, DecorateBiome, OreGen);
- every registered `IWorldGenerator`.

Under GT New Horizons 2.9.0-beta-2 (2026-10-23):
- No ReplaceBiomeBlocks listener.
- Caves are `MapGenCaves` with Hodgepodge's `StdLCG`.
- 29 world generators (GregTech's among them), all in populate.
- 92 realistic biomes, of which 45 wrap Biomes O' Plenty biomes.
