# MixinChunkProviderGenerate_Capture.java

Hands `Capture` the overworld generator's values at each stage boundary of
a chunk while a capture is running (`Capture.on()`). When no capture is on,
each injection is one static check.

| Section | Injected at |
| --- | --- |
| `BI10` | after the `PUTFIELD biomesForGeneration` in `func_147424_a`. This is after the store: the invoke's own return happens before it. The first 100 entries are taken, since the array is reused at 256 for the 16 x 16 biomes. |
| `OCT0` to `OCT3` | after the `PUTFIELD field_147425_f` in `func_147423_a`, the fourth and last octave array |
| `FLD ` | after the call to `func_147423_a` in `func_147424_a` |
| `SHAP` | at the return of `func_147424_a`, from its block-array argument |
| `BI16`, `SURF`, `META` | at every return of `replaceBlocksForBiome`, from its arguments |
| `FLAG` bit 0 | at its first return (ordinal 0), the early one taken when a Forge `ReplaceBiomeBlocks` handler denies |
