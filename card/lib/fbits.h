/* fbits.h: a double compared with a constant as one integer compare of its bit pattern, by IEEE-754's order. The
 * card has no FCOMI or FUCOMI, so an x87 compare there is FUCOM, FNSTSW and SAHF. See fbits.md. */
#ifndef FBITS_H
#define FBITS_H

#include <stdint.h>
#include <string.h>

/* The double's 64 bits as a signed integer. The store rounds an x87 value to double: the value Java has. */
static inline int64_t dbits(double d)
{
    int64_t b;
    memcpy(&b, &d, sizeof b);
    return b;
}

/* For any d but NaN. Non-negative doubles order as their bit patterns, and every negative one (-0.0 too) is a
 * negative integer, so against a constant c >= +0 the signed compare is the float compare. Negative doubles order
 * backwards (sign and magnitude) and lie above every non-negative one as unsigned integers, so against a constant
 * c < 0 the unsigned compare, reversed, is the float compare. Each call site says why NaN cannot reach it. */
static inline int d_gt0(double d) { return dbits(d) > 0; }
/* d < c for a constant c > 0 */
static inline int d_lt_pos(double d, double c) { return dbits(d) < dbits(c); }
/* d > c for a constant c < 0 */
static inline int d_gt_neg(double d, double c) { return (uint64_t)dbits(d) < (uint64_t)dbits(c); }

#endif
