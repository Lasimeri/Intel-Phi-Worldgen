# RwgCapture.java

Captures of RWG (Realistic World Gen alpha 1.5.2, GT New Horizons' locked
world type) for the card, and the RWG world state pwgd is sent
(`docs/rwg-capture-format.md`). RWG is read by reflection, so the mod needs
RWG only where it is installed.

- **`refusal`** says why the card cannot reproduce this world, or null. Three cases:
  - the generator is not RWG's;
  - its noise is not the Perlin `NoiseGeneratorWrapper` (`useOpenSimplex` is static state RWG sets from the world's saved data);
  - a realistic biome's class or terrain is not one `card/lib/rwg.c` has.
- **`worldBlob`** holds the seed, the Perlin tables, the three cell noises, the parabolic weights, the biome lists in RWG's order, and each realistic biome's rNoise kind and terrain parameters.
- **`capture`** calls the generator's own `getNewNoise` per chunk of the set and writes what it leaves.
  - After each call it clears `mapGenBiomes[0..255]`, as `provideChunk` does after the map gens, so the world's later chunks are unaffected.
  - Nothing is written into the world.
- **The rNoise kind** of each biome class was read from RWG's bytecode:
  - 37 classes return `this.terrain.generateNoise(...)` exactly;
  - 13 have their own code.
