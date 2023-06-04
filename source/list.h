#ifndef _NX_LIST_
#define _NX_LIST_

// Double linked list implementation

typedef struct {
   struct nx_list_t *next;
   struct nx_list_t *prev;
} nx_list_t;

#endif