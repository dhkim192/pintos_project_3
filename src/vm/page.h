#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdio.h>
#include <stdlib.h>
#include "kernel/hash.h"
#include "vm/frame.h"
#include <list.h>

#define BINARY_FILE             0
#define MEMORY_MAPPED_FILE      1
#define SWAPPING                2

extern struct lock lru_list_lock;

struct virtual_memory_entry {
    int virtual_memory;
    void * vaddr;
    void * kpage;
    bool writable;
    bool is_loaded;

    struct file * file;

    struct list_elem mmap_elem;
    
    size_t offset;
    size_t read_bytes;
    size_t zero_bytes;

    size_t swap_slot;

    struct hash_elem hash_elem;
};

hash_hash_func hash_func;
hash_less_func less_func; 
bool virtual_memory_init(struct hash * virtual_memory);
bool virtual_memory_entry_insert(struct hash * virtual_memory, struct virtual_memory_entry * entry);
bool virtual_memory_entry_delete(struct hash * virtual_memory, struct virtual_memory_entry * entry);
struct virtual_memory_entry * virtual_memory_entry_find(void * vaddr);
void destroy_virtual_memory_entry(struct hash_elem * elem, void *aux UNUSED);
void virtual_memory_entry_destroy(struct hash * virtual_memory);

bool lazy_load (void * vaddr);

#endif /* vm/page.h */