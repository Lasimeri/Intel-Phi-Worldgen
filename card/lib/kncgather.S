# kncgather.S: a 16-lane dword gather on the card's vector unit, out of line. See kncgather.md.
#
# void knc_gather16(int32_t *out, const int32_t *table, const int32_t *idx)
#     out[i] = table[idx[i]] for i in 0..15; out and idx 64-byte aligned.
#
# vpgatherdd completes only the lanes that share a cache line per issue and clears their mask bits, so it loops
# until the mask is empty (ISA 327364-001). The Intel-Phi-LLVM S3 series has no gather intrinsic, and inline asm
# cannot keep the vector registers it uses away from the compiler's (the KNC backend ignored zmm clobbers,
# 2026-10-22), so the gather is a call: every zmm and k register is caller-saved.
    .text
    .globl knc_gather16
    .type knc_gather16, @function
knc_gather16:
    vmovdqa32 (%rdx), %zmm1
    kxnor %k1, %k1
1:
    vpgatherdd (%rsi,%zmm1,4), %zmm0{%k1}
    kortest %k1, %k1
    jnz 1b
    vmovdqa32 %zmm0, (%rdi)
    ret
    .size knc_gather16, .-knc_gather16
    .section .note.GNU-stack,"",@progbits
