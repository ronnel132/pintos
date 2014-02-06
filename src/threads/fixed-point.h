/*! \file fixed-point.h
 *
 * Type definitions and routines for fixed-point arithmetic.
 * Use the type fixedpt for indicating a fixed-point variable.
 */

#ifndef FIXEDPT_H
#define FIXEDPT_H

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




#endif
