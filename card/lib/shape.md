# shape.h, shape.c

1.7.10's terrain shape in C, written operation for operation from the
decompiled Java so that the card produces the same bits. The checked
reference is the capture (`docs/capture-format.md`); `replay.c` compares.

| Function | Java it ports |
| --- | --- |
| `populate` | `NoiseGeneratorImproved.populateNoiseArray`, both branches: `ys == 1` is the 2D form (its first corner uses the 2D gradient tables `field_152384_h`/`_i`, the other three the 3D ones with y = 0, as in Java) |
| `noise_octaves` | `NoiseGeneratorOctaves.generateNoiseOctaves`: per octave the offset is floored with `floor_double_long`, wrapped with `% 16777216` (C's `%` truncates toward zero like Java's), and the scale halves |
| `shape_octaves` | the four calls at the start of `func_147423_a`; the 2D one goes through Java's 8-argument overload, which passes y = 10, sizes 5 x 1 x 5 and y scale 1.0, and drops its last argument |
| `shape_combine` | the rest of `func_147423_a`: the 5 x 5 biome blend in float, the depth noise, and the 33 heights in double |
| `shape_interp` | the interpolation of `func_147424_a`: stone above 0, water below height 63, else `null` |

## Parity rules

- **Rounding control:** the card's scalar floating point is x87. `replay.c` sets the precision control to 53 bits (`fldcw 0x027F`), so each double operation rounds as Java's does. The exponent range stays wider, which matters only for overflow and denormals, neither of which this computation reaches.
- **Float operations:** every one is wrapped in `rf`, a store to a `volatile float`. The x87 computes the operation to 53 bits; the store rounds it to 24. For a single +, -, * or / that equals the IEEE single result, because 53 >= 2 x 24 + 2. Without the store, LLVM keeps the value in a register and the next operation sees the unrounded value.
- **Expression order:** Java's left-to-right order is kept as written (`d * d * d * (...)` is `((d * d) * d) * (...)`), and `-ffp-contract=off` stops the host compiler fusing a multiply and an add.
- **The cache quirk:** `populate`'s 3D branch recomputes the four x/z interpolations (`d8` to `d11`) only at the first height of each vertical noise cell, using that height's fraction, and reuses them for the rest of the cell. Java does exactly this.
- **Block ids:** stone 1, still water 9, `null` 0xFFFF.

## Measured

Capture `s2a`, 91 chunks, on 2026-10-22: 0 chunks differ at any stage (the
octaves; the combine from the captured octaves; the interpolation from the
captured field; end to end), on the host and on the 7120A. The card's
timing, scalar, one chunk per thread:

| Threads | Chunks/s |
| --- | --- |
| 1 | 66.2 (15.09 ms a chunk) |
| 61 | 3,103 to 3,117 |
| 122 | 3,906 to 4,011 |
| 244 | 3,690 to 3,980 |

These are three runs of 91 x 100 chunks each; the logs are
`~/.cache/phiworldgen/s3-shape-card-*.log`.
