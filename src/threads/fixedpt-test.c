#define FIXEDPT_TEST
#include "fixed-point.h"
#include <stdio.h>

int main(void) {
	fixedpt x, y;
	int n;

	/* addition */
	x = 5;
	y = 9;
	printf("int x + y = %d + %d = %d\n", x, y, x + y);
	printf("fixedpt x + y = %d + %d = %d\n", int_to_fixedpt(x),
											int_to_fixedpt(y),
											fixedpt_add(int_to_fixedpt(x), int_to_fixedpt(y)));
	printf("fixedpt x + y = %d + %d = intzero %d\n", int_to_fixedpt(x),
											int_to_fixedpt(y),
											fixedpt_to_int_zero(fixedpt_add(int_to_fixedpt(x), int_to_fixedpt(y))));
	printf("fixedpt x + y = %d + %d = intnearest %d\n", int_to_fixedpt(x),
											int_to_fixedpt(y),
											fixedpt_to_int_nearest(fixedpt_add(int_to_fixedpt(x), int_to_fixedpt(y))));

	/* subtraction */
	printf("\nint x - y = %d - %d = %d\n", x, y, x - y);
	printf("fixedpt x - y = %d - %d = %d\n", int_to_fixedpt(x),
											int_to_fixedpt(y),
											fixedpt_sub(int_to_fixedpt(x), int_to_fixedpt(y)));
	printf("fixedpt x - y = %d - %d = intzero %d\n", int_to_fixedpt(x),
											int_to_fixedpt(y),
											fixedpt_to_int_zero(fixedpt_sub(int_to_fixedpt(x), int_to_fixedpt(y))));
	printf("fixedpt x - y = %d - %d = intnearest %d\n", int_to_fixedpt(x),
											int_to_fixedpt(y),
											fixedpt_to_int_nearest(fixedpt_sub(int_to_fixedpt(x), int_to_fixedpt(y))));

	/* multiplication */
	printf("\nint x * y = %d * %d = %d\n", x, y, x * y);
	printf("fixedpt x * y = %d * %d = %d\n", int_to_fixedpt(x),
											int_to_fixedpt(y),
											fixedpt_mul(int_to_fixedpt(x), int_to_fixedpt(y)));
	printf("fixedpt x * y = %d * %d = intzero %d\n", int_to_fixedpt(x),
											int_to_fixedpt(y),
											fixedpt_to_int_zero(fixedpt_mul(int_to_fixedpt(x), int_to_fixedpt(y))));
	printf("fixedpt x * y = %d * %d = intnearest %d\n", int_to_fixedpt(x),
											int_to_fixedpt(y),
											fixedpt_to_int_nearest(fixedpt_mul(int_to_fixedpt(x), int_to_fixedpt(y))));

	/* division */
	printf("\nint x / y = %d / %d = %d\n", x, y, x / y);
	printf("fixedpt x / y = %d / %d = %d\n", int_to_fixedpt(x),
											int_to_fixedpt(y),
											fixedpt_div(int_to_fixedpt(x), int_to_fixedpt(y)));
	printf("fixedpt x / y = %d / %d = intzero %d\n", int_to_fixedpt(x),
											int_to_fixedpt(y),
											fixedpt_to_int_zero(fixedpt_div(int_to_fixedpt(x), int_to_fixedpt(y))));
	printf("fixedpt x / y = %d / %d = intnearest %d\n", int_to_fixedpt(x),
											int_to_fixedpt(y),
											fixedpt_to_int_nearest(fixedpt_div(int_to_fixedpt(x), int_to_fixedpt(y))));

	return 0;
}