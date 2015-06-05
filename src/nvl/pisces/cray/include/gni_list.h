
/*
 *
 * Simple doubly linked list implementation used in both uGNI and kGNI.
 *
 * Copyright 2011 Cray Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License as published by the 
 * Free Software Foundation; either version 2 of the License, 
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software Foundation, Inc., 
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef _GNI_LIST_H
#define _GNI_LIST_H

typedef struct gni_list {
	struct gni_list *n;
	struct gni_list *p;
} gni_list_t;

#define GNI_LIST_INIT(name) { &(name), &(name) }

#define GNI_LIST(list_elm)				\
	gni_list_t name = GNI_LIST_INIT(list_elm)

#define INIT_GNI_LIST(list_ptr) do {			\
	(list_ptr)->n = (list_ptr);			\
	(list_ptr)->p = (list_ptr);			\
} while (0)

/* queue style list add */
static inline void
gni_list_add_tail(gni_list_t *new_elm, gni_list_t *list_ptr)
{
	new_elm->n = list_ptr;
	new_elm->p = list_ptr->p;
	list_ptr->p   = new_elm;
	new_elm->p->n = new_elm;
}

/* remove an arbitrary element from it's list */
static inline void
gni_list_del(gni_list_t *old_elm)
{
	old_elm->p->n = old_elm->n;
	old_elm->n->p = old_elm->p;
}

/* remove an arbitrary element from it's list, initialize the removed element
 * to evaluate as an empty list */
static inline void
gni_list_del_init(gni_list_t *old_elm)
{
	old_elm->p->n = old_elm->n;
	old_elm->n->p = old_elm->p;
	INIT_GNI_LIST(old_elm);
}

/* return non-zero if the element is part of a list */
static inline int gni_list_empty(const gni_list_t *test_elm)
{
	return test_elm->n == test_elm;
}

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

#ifndef container_of
#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})
#endif

#define gni_list_iterate(iter, list_ptr, member)			\
	for (iter = container_of((list_ptr)->n, typeof(*iter), member);	\
	     &iter->member != (list_ptr);				\
	     iter = container_of(iter->member.n, typeof(*iter), member))

/* version of above that supports calling gni_list_del() inside loop */
#define gni_list_iterate_safe(iter, iter_next, list_ptr, member)	\
	for (iter = container_of((list_ptr)->n, typeof(*iter), member),	\
	     iter_next = container_of(iter->member.n, typeof(*iter), member);	\
	     &iter->member != (list_ptr);					\
	     iter = iter_next,							\
	     iter_next = container_of(iter->member.n, typeof(*iter), member))
#endif
