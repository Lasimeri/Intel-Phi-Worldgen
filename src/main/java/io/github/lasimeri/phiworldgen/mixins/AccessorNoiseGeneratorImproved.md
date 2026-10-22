# AccessorNoiseGeneratorImproved.java

Exposes the private `permutations` of one Perlin octave
(`NoiseGeneratorImproved`): 512 ints, the shuffled 0..255 written twice so
that index + 255 never needs a wrap. With the public `xCoord`, `yCoord` and
`zCoord` offsets it is the octave's whole state (the decompiled constructor
draws the three offsets with `nextDouble() * 256.0`, then the shuffle).
