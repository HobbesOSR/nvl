/*
        A set of bit operations for GNI shared user and kernel code use.

        Copyright (C) 2011 Cray Inc. All Rights Reserved.

        This program is free software; you can redistribute it and/or modify it 
        under the terms of the GNU General Public License as published by the 
        Free Software Foundation; either version 2 of the License, 
        or (at your option) any later version.

        This program is distributed in the hope that it will be useful, 
        but WITHOUT ANY WARRANTY; without even the implied warranty of 
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
        See the GNU General Public License for more details.

        You should have received a copy of the GNU General Public License 
        along with this program; if not, write to the Free Software Foundation, Inc., 
        51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

#ifndef _GNI_BITOPS_H
#define _GNI_BITOPS_H

#ifdef __KERNEL__

#define gni_bitmap_weight	bitmap_weight
#define gni_test_bit		test_bit
#define gni_set_bit		set_bit

#else

#define BITS_PER_LONG		(sizeof(unsigned long) * 8)

/* gcc_bitmap_weight - Find the number of 1s in a bitmap. */
static inline int
gni_bitmap_weight(const unsigned long *src, int nbits)
{
	int i;
	int weight = 0;
	int full_qws = nbits / BITS_PER_LONG;

	for (i = 0; i < full_qws; i++)
		weight += __builtin_popcountl(src[i]);

	if (nbits % BITS_PER_LONG) {
		unsigned long mask = (1UL << ((nbits) % BITS_PER_LONG)) - 1;
		weight += __builtin_popcountl(src[i] & mask);
	}

	return weight;
}

/* gcc_test_bit - Test if a bit is set in a bitmap. */
static inline int
gni_test_bit(int nr, const void * addr)
{
	return ((unsigned char *) addr)[nr >> 3] & (1U << (nr & 7));
}

static inline void
gni_set_bit(int nr, void * addr)
{
	asm("btsl %1,%0" : "+m" (*(uint32_t *)addr) : "Ir" (nr));
}

#endif

#endif
