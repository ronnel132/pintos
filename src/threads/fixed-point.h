/*! \file fixed-point.h
 *
 * Type definitions and routines for fixed-point arithmetic.
 * Use the type fixedpt for indicating a fixed-point variable.
 */


/*! P.Q Fixed point arithmetic definitions. */
#define FIXED_POINT_P 17
#define FIXED_POINT_Q 14

/* Bit shift operation is probably evaluated at compile-time so as to have no
   performance hit. */
#define FIXED_POINT_F (1 << FIXED_POINT_Q)

#define MAX_FIXEDPT (((1 << 31) - 1)/(1 << FIXED_POINT_Q))


/*! A fixed point type is just an integer */
typedef int fixedpt;


/* Fixed point function prototypes */
fixedpt int_to_fixedpt(int n);
int fixedpt_to_int_zero(fixedpt x);
int fixedpt_to_int_nearest(fixedpt x);

fixedpt fixedpt_add(fixedpt x, fixedpt y);
fixedpt fixedpt_sub(fixedpt x, fixedpt y);
fixedpt fixedpt_mul(fixedpt x, fixedpt y);
fixedpt fixedpt_div(fixedpt x, fixedpt y);


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
