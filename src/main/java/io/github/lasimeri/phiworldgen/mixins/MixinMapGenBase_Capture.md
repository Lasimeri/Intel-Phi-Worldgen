# MixinMapGenBase_Capture.java

At the return of `MapGenBase.func_151539_a`, while a capture is on, hands
`Capture` the block array:

- as `CAVE` when the generator is exactly `MapGenCaves`;
- as `RAVN` when it is exactly `MapGenRavine`.

The structure generators run through the same method. They pass the blocks
unchanged (structures place their pieces in `populate`), and
`recreateStructures` passes `null`, so both are skipped.
