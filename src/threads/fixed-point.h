/*! \file fixed-point.h
 *
 * Type definitions and routines for fixed-point arithmetic.
 * Use the type fixedpt for indicating a fixed-point variable.
 */

/*! P.Q Fixed point arithmetic definitions. */
#define FIXED_POINT_P 17
#define FIXED_POINT_Q 14
#define FIXED_POINT_F 1 << FIXED_POINT_Q

/*! A fixed point type is just an integer */
typedef fixedpt int;



fixedpt int_to_fixedpt(int n);
int fixedpt_to_int_zero(fixedpt x);
int fixedpt_to_int_nearest(fixedpt x);

fixedpt fixedpt_add(fixedpt x, fixedpt y);
fixedpt fixedpt_sub(fixedpt x, fixedpt y);
fixedpt fixedpt_mul(fixedpt x, fixedpt y);
fixedpt fixedpt_div(fixedpt x, fixedpt y);


/*! Converts an integer to fixed-point type. */
fixedpt int_to_fixedpt(int n) {
    return n * FIXED_POINT_F;
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


fixedpt fixedpt_add(fixedpt x, fixedpt y) {
    return x + y;
}

fixedpt fixedpt_sub(fixedpt x, fixedpt y) {
    return x - y;
}

fixedpt fixedpt_mul(fixedpt x, fixedpt y) {
    return ((int64_t) x) * y / FIXED_POINT_F;
}

fixedpt fixedpt_div(fixedpt x, fixedpt y) {
    return ((int64_t) x) * FIXED_POINT_F / y;
}
