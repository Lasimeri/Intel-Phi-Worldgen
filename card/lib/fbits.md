# fbits.h

A double compared with a constant as one integer compare of its 64-bit
pattern, using IEEE-754's encoding order. It is the double form of the trick
phi-vision uses for ReLU (`vpmaxsd` on float bits).

**Why it can pay on the card:** Knights Corner deletes FCOMI, FCOMIP,
FUCOMI, FUCOMIP, FCMOVcc and CMOV (`~/Intel Phi 7120A/docs/research/isa-deletions.md`).
So every x87 compare in a card build is FUCOM, then FNSTSW, then SAHF before
the branch. The integer form stores the value and compares the stored bits
instead.

| Helper | Float compare | Integer compare | Holds for |
| --- | --- | --- | --- |
| `d_gt0(d)` | `d > 0.0` | `bits(d) > 0`, signed | every d but NaN; -0.0 is INT64_MIN |
| `d_lt_pos(d, c)` | `d < c`, c > 0 | `bits(d) < bits(c)`, signed | every d but NaN; negatives are negative integers |
| `d_gt_neg(d, c)` | `d > c`, c < 0 | `bits(d) < bits(c)`, unsigned | every d but NaN; negatives order backwards |

**Why these are exact:**
- Non-negative doubles order as their bit patterns: biased exponent above the fraction, no duplicate encodings.
- The sign bit makes every negative double a negative signed integer, and puts it above every non-negative one as an unsigned integer.
- Among negatives, a larger magnitude is a larger pattern, which is why `d_gt_neg` compares unsigned and reversed.

Each call site states why NaN cannot reach it.

## Where it is used

- `shape_interp`: `(d15 += d16) > 0.0`, the stone test, 32,768 times per chunk.
- Compiled code: `faddp; fstpl -8(%rsp); cmpq $0,-8(%rsp)`.
- `objdump` of pwgd: 36 FUCOM/FNSTSW/SAHF in `shape_interp` before, 0 after. FNSTSW in the binary went from 88 to 76.

## Tried and taken out: the carvers

The five compares in the carvers' per-block loops were converted too:
- caves: the column test `< 1.0`, plus `> -0.7` and `< 1.0` per y;
- ravines: the column test and the per-y test.

The caves stage then measured **2.6 to 4.7 % slower** (old and new ranges do not overlap). The loop there stores a value straight after an `fdiv` and reloads it as an integer:

```
fdivrp; fstl m; cmp %rbx,m; ...; fstpl m; cmp %r11,m
```

On this in-order core that loses to FUCOM+FNSTSW+SAHF. The likely reason is the store-to-load round trip behind the divide, but that is not isolated. `carve.c` keeps the float compares.

## Measured

Every capture passes the replay gate, host and card:
- s1, s2, s2a, s3, c1, c2, c3, g1, g2, g3;
- every stage, 0 chunks differ;
- c1 to c3 also compare the whole chunk.

Stage times are verification-side (replay `whole`, 1 card thread, 2 rounds), 4 runs each of old (420329d) and new, interleaved.

**Interp only (this version):**

| Capture | shape, old ms | shape, new ms | change |
| --- | --- | --- | --- |
| c1 | 5.302 [5.282..5.324] | 5.249 (3 runs) | -1.0 % |
| c2 | 5.279 [5.272..5.286] | 5.254 [5.231..5.291] | -0.5 % |
| c3 | 5.281 [5.258..5.307] | 5.249 [5.228..5.275] | -0.6 % |

- c1's fourth new run was 5 % lower in every stage, including stages whose code did not change, so it is left out.
- Stages that did not change moved by up to 1.6 % between runs.
- So the whole chain (about 15 ms a chunk) does not move outside that noise.

**Live, on new seeds** (2026-10-23, `~/.cache/phiworldgen/ab-pwgd.sh`):
- Setup: card on in both runs; pwgd f88dffe0 (420329d) against 36102a3f (this version); pregen radius 16; a new seed per pair, order alternating.
- Control pair (old against old): 332.6 and 328.9 chunks/s; card compute 17.886 and 18.280 ms a chunk.

| Pair | chunks/s old | chunks/s new | new/old | compute ms old | compute ms new | new/old |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 498.6 | 496.3 | 0.9954 | 18.085 | 17.724 | 0.9800 |
| 2 | 254.0 | 251.6 | 0.9906 | 17.541 | 17.866 | 1.0185 |
| 3 | 483.6 | 485.4 | 1.0037 | 17.918 | 17.812 | 0.9941 |
| 4 | 221.1 | 219.9 | 0.9946 | 20.938 | 21.136 | 1.0095 |
| 5 | 695.5 | 723.4 | 1.0401 | 17.994 | 18.303 | 1.0172 |

- The worlds are byte-identical in every pair.
- Pair 4 took the shape-only path (0 whole chunks) in both runs.
- Live, there is **no measurable gain**: new is faster in 2 of 5 pairs on either metric, and the spread is inside the control's 2.2 %.

**Kept (2026-10-23)**, on the condition that world generation stays exact.
Checked again on the tree that keeps it: the replay on the host and on the
card, all ten captures, every stage 0 chunks differ.
