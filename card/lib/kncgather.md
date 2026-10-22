# kncgather.S

`knc_gather16(out, table, idx)`: `out[i] = table[idx[i]]` for 16 lanes, a
`vpgatherdd` looped until its mask is empty (a KNC gather completes only the
lanes that share one cache line per issue).

It is an out-of-line function on purpose. The Intel-Phi-LLVM S3 series has
no gather intrinsic, and the same gather as inline asm with `zmm30`,
`zmm31` and `k7` in its clobber list was wrong inside `vshape.c`: the KNC
backend kept accumulators in those registers across the asm (510 of 825
values differed). As a call, every vector and mask register is caller-saved.
