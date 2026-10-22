# AccessorNoiseGeneratorOctaves.java

Exposes `NoiseGeneratorOctaves.generatorCollection`, the private array of
one `NoiseGeneratorImproved` per octave, in the order `generateNoiseOctaves`
uses them (octave 0 at full scale, each later one at half the previous
scale). The capture reads each octave's state through it
(`AccessorNoiseGeneratorImproved`); the verification writes captured state
into a provider built from another seed.
