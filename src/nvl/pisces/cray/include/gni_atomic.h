/*
        A set of atomic operations using the GCC-builtin atomics.

        Copyright 2011 Cray Inc. All Rights Reserved.

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

#ifndef _GNI_ATOMIC_H
#define _GNI_ATOMIC_H

/*
 * A set of atomic operations using the GCC-builtin atomics, listed here:
 *    http://gcc.gnu.org/onlinedocs/gcc/Atomic-Builtins.html
 * Interface below is API-compatible with Linux atomic operations.
 */

#ifndef __KERNEL__
/**
 * Atomic type.
 */
typedef struct {
    volatile int counter;
} atomic_t;

typedef struct {
    volatile long counter;
} atomic64_t;

#define ATOMIC_INIT(i)  { (i) }
#define atomic_set(v,i) (((v)->counter) = (i))
#define atomic_read(v)  ((v)->counter)
#define atomic64_set(v,i) atomic_set(v,i)
#define atomic64_read(v)  atomic_read(v)
#endif /* __KERNEL__ */

#define gcc_atomic_read  atomic_read
#define gcc_atomic_set   atomic_set
#define gcc_atomic64_read atomic64_read
#define gcc_atomic64_set  atomic64_set

static inline int gcc_atomic_add(int i, atomic_t *v)
{
         return __sync_add_and_fetch(&v->counter, i);
}

static inline void gcc_atomic_sub(int i, atomic_t *v)
{
        (void)__sync_sub_and_fetch(&v->counter, i);
}

static inline int gcc_atomic_sub_and_test(int i, atomic_t *v)
{
        return !(__sync_sub_and_fetch(&v->counter, i));
}

static inline void gcc_atomic_inc(atomic_t *v)
{
       (void)__sync_fetch_and_add(&v->counter, 1);
}

static inline int gcc_atomic_inc_return(atomic_t *v)
{
       return __sync_fetch_and_add(&v->counter, 1);
}

static inline void gcc_atomic_dec(atomic_t *v)
{
       (void)__sync_fetch_and_sub(&v->counter, 1);
}

static inline int gcc_atomic_dec_return(atomic_t *v)
{
       return __sync_fetch_and_sub(&v->counter, 1);
}

static inline int gcc_atomic_dec_and_test(atomic_t *v)
{
       return !(__sync_sub_and_fetch(&v->counter, 1));
}

static inline int gcc_atomic_inc_and_test(atomic_t *v)
{
      return !(__sync_add_and_fetch(&v->counter, 1));
}

static inline int gcc_atomic_add_negative(int i, atomic_t *v)
{
       return (__sync_add_and_fetch(&v->counter, i) < 0);
}

static inline int gcc_atomic_cas(atomic_t *v, int oldval, int newval)
{
        return __sync_val_compare_and_swap(&v->counter, oldval, newval);
}

static inline long gcc_atomic64_cas(atomic64_t *v, long oldval, long newval)
{
	return __sync_val_compare_and_swap(&v->counter, oldval, newval);
}

static inline int gcc_atomic_dec_if_positive(atomic_t *v)
{
	int curr;

_retry_until_swapped:
	curr = gcc_atomic_read(v);
	if (curr > 0) {
		if (gcc_atomic_cas(v, curr, curr - 1) != curr) {
			/* contention for resource - RETRY */
			goto _retry_until_swapped;
		} else {
			/* swap succeeded - SUCCESS */
			return (curr - 1);
		}
	} else {
		/* dec_if_postive failed */
		return -1;
	}
}
#endif
