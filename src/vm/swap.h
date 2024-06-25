#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "page.h"

void swap_table_init (void);
void swap_table_destroy (tid_t tid, void *upage);
off_t swap_table_swap_in (tid_t tid, void *upage, void *kpage);
void swap_table_swap_out (tid_t tid, void *upage, void *kpage, off_t position);
bool swap_table_is_swapped (tid_t tid, void *upage);


#endif /* vm/swap.h */