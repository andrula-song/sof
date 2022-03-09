// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

/* Binary GCD algorithm
 * https://en.wikipedia.org/wiki/Binary_GCD_algorithm
 */

#include <sof/audio/format.h>
#include <sof/math/numbers.h>
#include <stdint.h>

/* This function returns the greatest common divisor of two numbers
 * If both parameters are 0, gcd(0, 0) returns 0
 * If first parameters is 0 or second parameter is 0, gcd(0, b) returns b
 * and gcd(a, 0) returns a, because everything divides 0.
 */

int gcd(int a, int b)
{
	if (a == 0)
		return b;

	if (b == 0)
		return a;

	/* If the numbers are negative, convert them to positive numbers
	 * gcd(a, b) = gcd(-a, -b) = gcd(-a, b) = gcd(a, -b)
	 */

	if (a < 0)
		a = -a;

	if (b < 0)
		b = -b;

	int aux;
	int k;

	/* Find the greatest power of 2 that devides both a and b */
	for (k = 0; ((a | b) & 1) == 0; k++) {
		a >>= 1;
		b >>= 1;
	}

	/* divide by 2 until a becomes odd */
	while ((a & 1) == 0)
		a >>= 1;

	do {
		/*if b is even, remove all factors of 2*/
		while ((b & 1) == 0)
			b >>= 1;

		/* both a and b are odd now. Swap so a <= b
		 * then set b = b - a, which is also even
		 */
		if (a > b) {
			aux = a;
			a = b;
			b = aux;
		}

		b = b - a;

	} while (b != 0);

	/* restore common factors of 2 */
	return a << k;
}


#if CONFIG_NUMBERS_VECTOR_FIND

/* This function searches from vec[] (of length vec_length) integer values
 * of n. The indexes to equal values is returned in idx[]. The function
 * returns the number of found matches. The max_results should be set to
 * 0 (or negative) or vec_length get all the matches. The max_result can be set
 * to 1 to receive only the first match in ascending order. It avoids need
 * for an array for idx.
 */
int find_equal_int16(int16_t idx[], int16_t vec[], int n, int vec_length,
	int max_results)
{
	int nresults = 0;
	int i;

	for (i = 0; i < vec_length; i++) {
		if (vec[i] == n) {
			idx[nresults++] = i;
			if (nresults == max_results)
				break;
		}
	}

	return nresults;
}

/* Return the smallest value found in the vector */
int16_t find_min_int16(int16_t vec[], int vec_length)
{
	int i;
	int min = vec[0];

	for (i = 1; i < vec_length; i++)
		min = (vec[i] < min) ? vec[i] : min;

	return min;
}

/* Return the largest absolute value found in the vector. Note that
 * smallest negative value need to be saturated to preset as int32_t.
 */
int32_t find_max_abs_int32(int32_t vec[], int vec_length)
{
	int i;
	int64_t amax = (vec[0] > 0) ? vec[0] : -vec[0];

	for (i = 1; i < vec_length; i++) {
		amax = (vec[i] > amax) ? vec[i] : amax;
		amax = (-vec[i] > amax) ? -vec[i] : amax;
	}

	return SATP_INT32(amax); /* Amax is always a positive value */
}

#endif /* CONFIG_VECTOR_FIND */

#if CONFIG_NUMBERS_NORM

/* Count the left shift amount to normalize a 32 bit signed integer value
 * without causing overflow. Input value 0 will result to 31.
 */
int norm_int32(int32_t val)
{
	int c = 0;

	/* count number of bits c that val can be right-shifted arithmetically
	 * until there is -1 (if val is negative) or 0 (if val is positive)
	 * norm of val will be 31-c
	 */
	for (; val != -1 && val != 0; c++)
		val >>= 1;

	return 31 - c;
}

#endif /* CONFIG_NORM */

/**
 * Basic CRC-32 implementation, based on pseudo-code from
 * https://en.wikipedia.org/wiki/Cyclic_redundancy_check#CRC-32_algorithm
 * 0xEDB88320 is the reversed polynomial representation
 */
uint32_t crc32(uint32_t base, const void *data, uint32_t bytes)
{
	uint32_t crc = ~base;
	uint32_t cur;
	int i;
	int j;

	for (i = 0; i < bytes; ++i) {
		cur = (crc ^ ((const uint8_t *)data)[i]) & 0xFF;

		for (j = 0; j < 8; ++j)
			cur = (cur & 1) ? (cur >> 1) ^ 0xEDB88320 : cur >> 1;

		crc = cur ^ (crc >> 8);
	}

	return ~crc;
}

/**
 * Find the last (most-significant) set bit in word x,
 * https://www.kernel.org/doc/htmldocs/kernel-api/API-fls.html
 * fls(0) is 0, fls(1) is 1, fls(0x80000000) = 32.
 */
int fls(int x)
{
	int r = 32;

	if (!x)
		return 0;
	if (!(x & 0xffff0000u)) {
		x <<= 16;
		r -= 16;
	}
	if (!(x & 0xff000000u)) {
		x <<= 8;
		r -= 8;
	}
	if (!(x & 0xf0000000u)) {
		x <<= 4;
		r -= 4;
	}
	if (!(x & 0xc0000000u)) {
		x <<= 2;
		r -= 2;
	}
	if (!(x & 0x80000000u)) {
		x <<= 1;
		r -= 1;
	}
	return r;
}

/**
 * Return the frames that meet the align requirement of both byte_align and
 * frame_align_req.
 * @param byte_align Processing byte alignment requirement.
 * @param frame_align_req Processing frames alignment requirement.
 * @param frame_size Size of the frame in bytes.
 * @return frame number.
 */
uint32_t frame_align(uint32_t byte_align, uint32_t frame_align_req,
		     uint32_t frame_size)
{
	/** calculate the frame number meets the requirement of byte_align
	 * alignment
	 */
	uint32_t frame_num = byte_align / gcd(byte_align, frame_size);

	/** return the lcm of frame_num and frame_align_req*/
	return frame_align_req * frame_num / gcd(frame_num, frame_align_req);
}
