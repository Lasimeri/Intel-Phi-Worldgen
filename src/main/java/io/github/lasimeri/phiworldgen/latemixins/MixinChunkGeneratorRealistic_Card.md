# MixinChunkGeneratorRealistic_Card.java

The card in RWG worlds: at the head of `generateTerrain` it takes the card's
answer for the chunk (`CardShape.fetch`, which also asks for the ring around
it) and, when the answer is RWG's height stage, applies it (`RwgCard.apply`)
and cancels the CPU's work. A chunk never asked for, an answer later than
`waitMs`, or no card leaves `generateTerrain` to RWG. Only the generator the
card was given (`CardClient.provider`) is served.

Check mode keeps the answer and, at `generateTerrain`'s return, compares it
with what the CPU made (`RwgCard.compare`); the CPU's arrays are the ones
used.

A late mixin (`LateMixins`), written in RWG's shipped SRG names with remap
off; RWG's jar is a compile-only dependency.
