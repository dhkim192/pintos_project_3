#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stddef.h>

void swap_init(size_t size);
void swap_in(size_t used_index, void *kaddr);
size_t swap_out(void * kaddr);
void swap_clear(size_t swap_slot);

#endif /* vm/swap.h */