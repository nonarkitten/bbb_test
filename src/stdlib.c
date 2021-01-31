#include <stdlib.h>
#include <stdint.h>

#if 0

typedef uint32_t uint32x2_t __attribute__ ((vector_size (8)));
typedef  int32_t  int32x2_t __attribute__ ((vector_size (8)));

uint32_t __aeabi_uidiv(uint32_t numerator, uint32_t denominator) {
	return (uint32_t)((double)numerator / (double)denominator);
}

uint32x2_t __aeabi_uidivmod(uint32_t numerator, uint32_t denominator) {
    double d = (double)denominator;
    uint32_t q = (uint32_t)((double)numerator / d);
    uint32_t r = numerator - ((double)q * d);
    uint32x2_t ret = { q, r };
    return ret;
}

int32_t __aeabi_idiv(int32_t numerator, int32_t denominator) {
	return (int32_t)((double)numerator / (double)denominator);
}

int32x2_t __aeabi_idivmod(int32_t numerator, int32_t denominator) {
    double d = (double)denominator;
    int32_t q = (int32_t)((double)numerator / d);
    int32_t r = numerator - ((double)q * d);
    int32x2_t ret = { q, r };
    return ret;
}

#endif

div_t div (int num, int denom) {
	div_t r;

	r.quot = num / denom;
	r.rem = num % denom;
	/*
	 * The ANSI standard says that |r.quot| <= |n/d|, where
	 * n/d is to be computed in infinite precision.  In other
	 * words, we should always truncate the quotient towards
	 * 0, never -infinity or +infinity.
	 *
	 * Machine division and remainer may work either way when
	 * one or both of n or d is negative.  If only one is
	 * negative and r.quot has been truncated towards -inf,
	 * r.rem will have the same sign as denom and the opposite
	 * sign of num; if both are negative and r.quot has been
	 * truncated towards -inf, r.rem will be positive (will
	 * have the opposite sign of num).  These are considered
	 * `wrong'.
	 *
	 * If both are num and denom are positive, r will always
	 * be positive.
	 *
	 * This all boils down to:
	 *	if num >= 0, but r.rem < 0, we got the wrong answer.
	 * In that case, to get the right answer, add 1 to r.quot and
	 * subtract denom from r.rem.
	 *      if num < 0, but r.rem > 0, we also have the wrong answer.
	 * In this case, to get the right answer, subtract 1 from r.quot and
	 * add denom to r.rem.
	 */
	if (num >= 0 && r.rem < 0) {
		++r.quot;
		r.rem -= denom;
	}
	else if (num < 0 && r.rem > 0) {
		--r.quot;
		r.rem += denom;
	}
	return (r);
}

