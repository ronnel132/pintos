#include <stddef.h>
#include <debug.h>
#include <stdint.h>
#include "threads/fixed-point.h"

/*! Converts an integer to fixed-point type. */
fixedpt int_to_fixedpt(int n) {
    ASSERT(n <= MAX_FIXEDPT);

    return ((fixedpt) (n * FIXED_POINT_F));
}

/*! Converts a fixed-point number to an integer.
    Rounds towards 0.
    E.g. -3.9 -> -3
          2.1 ->  2
          2.9 ->  2 */
int fixedpt_to_int_zero(fixedpt x) {
    return x / FIXED_POINT_F;
}

/*! Converts a fixed-point number to an integer.
    Rounds canonically.
    E.g. -3.9 -> -4
          2.1 ->  2
          2.9 ->  3 */
int fixedpt_to_int_nearest(fixedpt x) {
    if (x >= 0) {
        return (x + FIXED_POINT_F / 2) / FIXED_POINT_F;
    } else {
        return (x - FIXED_POINT_F / 2) / FIXED_POINT_F;
    }
}

/*! Adds two fixed-point numbers. */
fixedpt fixedpt_add(fixedpt x, fixedpt y) {
    return x + y;
}

/*! Subtracts two fixed-point numbers. */
fixedpt fixedpt_sub(fixedpt x, fixedpt y) {
    return x - y;
}

/*! Multiplies two fixed-point numbers. */
fixedpt fixedpt_mul(fixedpt x, fixedpt y) {
    return ((int64_t) x) * y / FIXED_POINT_F;
}

/*! Divides two fixed-point numbers. */
fixedpt fixedpt_div(fixedpt x, fixedpt y) {
    return ((int64_t) x) * FIXED_POINT_F / y;
}