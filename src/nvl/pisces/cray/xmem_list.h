#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include "xemem.h"
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>


struct xpmem_seg
{
  xemem_segid_t segid;
  uint64_t address;
  uint64_t length;
  struct xemem_seg* next;
};


struct memseg_list
{
  struct xpmem_seg* head;
  struct xpmem_seg* tail;
};


struct memseg_list* list_add_element( struct memseg_list*, xemem_segid_t*, uint64_t, uint64_t);
struct memseg_list* list_remove_element( struct memseg_list*);


struct memseg_list* list_new(void);
struct memseg_list* list_free( struct memseg_list* );

void list_print( const struct memseg_list* );
void list_print_element(const struct xpmem_seg* );
xemem_segid_t  list_find_segid_by_vaddr(const struct memseg_list*, uint64_t );
uint64_t  list_find_vaddr_by_segid(const struct memseg_list*, xemem_segid_t* );
/*

int main(void)
{
  struct memseg_list*  mt = NULL;

  mt = list_new();
  xemem_segid_t*	segid1, segid2, segid3, segid4;
  list_add_element(mt, segid1, 0x5000, 100);
  list_add_element(mt, segid2, 0x6000, 100);
  list_add_element(mt, segid3, 0x7000, 100);
  list_add_element(mt, segid4, 0x8000, 100);
  
  list_print(mt);

  list_find_segid_by_vaddr(mt, 0x5000);

  list_remove_element(mt);
  list_print(mt);

  list_free(mt);   
  free(mt);       
  mt = NULL;     

  list_print(mt);

  return 0;
}
*/
/* Will always return the pointer to memseg_list */
struct memseg_list* list_add_element(struct memseg_list* s, xemem_segid_t *segid, uint64_t addr, uint64_t len)
{
  struct xpmem_seg* p = malloc( 1 * sizeof(*p) );

  if( NULL == p )
    {
      fprintf(stderr, "IN %s, %s: malloc() failed\n", __FILE__, "list_add");
      return s; 
    }

  p->segid = *segid;
  p->address = addr;
  p->length = len;
  p->next = NULL;

	fprintf(stderr, "add a new seg to list segid %llu, address %p\n", *segid, addr);

  if( NULL == s )
    {
      printf("Queue not initialized\n");
      free(p);
      return s;
    }
  else if( NULL == s->head && NULL == s->tail )
    {
      /* printf("Empty list, adding p->num: %d\n\n", p->num);  */
      s->head = s->tail = p;
      return s;
    }
  else if( NULL == s->head || NULL == s->tail )
    {
      fprintf(stderr, "There is something seriously wrong with your assignment of head/tail to the list\n");
      free(p);
      return NULL;
    }
  else
    {
      /* printf("List not empty, adding element to tail\n"); */
      s->tail->next =  p;
      s->tail = p;
    }

  return s;
}


/* This is a queue and it is FIFO, so we will always remove the first element */
struct memseg_list* list_remove_element( struct memseg_list* s )
{
  struct xpmem_seg* h = NULL;
  struct xpmem_seg* p = NULL;

  if( NULL == s )
    {
      printf("List is empty\n");
      return s;
    }
  else if( NULL == s->head && NULL == s->tail )
    {
      printf("Well, List is empty\n");
      return s;
    }
  else if( NULL == s->head || NULL == s->tail )
    {
      printf("There is something seriously wrong with your list\n");
      printf("One of the head/tail is empty while other is not \n");
      return s;
    }

  h = s->head;
  p = h->next;
  free(h);
  s->head = p;
  if( NULL == s->head )  s->tail = s->head;   /* The element tail was pointing to is free(), so we need an update */

  return s;
}
  

/* ---------------------- small helper fucntions ---------------------------------- */
struct memseg_list* list_free( struct memseg_list* s )
{
  while( s->head )
    {
      list_remove_element(s);
    }

  return s;
}

struct memseg_list* list_new(void)
{
  struct memseg_list* p = malloc( 1 * sizeof(*p));

  if( NULL == p )
    {
      fprintf(stderr, "LINE: %d, malloc() failed\n", __LINE__);
    }

  p->head = p->tail = NULL;
  
  return p;
}


void list_print( const struct memseg_list* ps )
{
  struct xpmem_seg* p = NULL;

  if( ps )
    {
      for( p = ps->head; p; p = p->next )
	{
	  list_print_element(p);
	}
    }

  printf("------------------\n");
}

xemem_segid_t   list_find_segid_by_vaddr(const struct memseg_list* ps, uint64_t addr)
{

  struct xpmem_seg* p = NULL;

  if( ps )
    {
      for( p = ps->head; p; p = p->next )
	{
		if((p->address == addr) || ((addr > p->address) && (addr <  p->address + p->length)))
		{
			printf("found %p segid %llu\n", addr, p->segid);
			return (p->segid);
		}
			
	}
	return -1;
    }

}


uint64_t   list_find_vaddr_by_segid(const struct memseg_list* ps, xemem_segid_t  *segid)
{

  struct xpmem_seg* p = NULL;

  if( ps )
    {
      for( p = ps->head; p; p = p->next )
	{
		if(p->segid == *segid)
		{
			printf("found segid %llu\n", segid);
			return (p->address);
		}
			
	}
	return -1;
    }

}

void list_print_element(const struct xpmem_seg* p )
{
  if( p ) 
    {
      printf("Address = %p\n", p->address);
      printf("segid = %llu\n", p->segid);
    }
  else
    {
      printf("Can not print NULL struct \n");
    }
}
