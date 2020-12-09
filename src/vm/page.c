#include <stdio.h>
#include <string.h>
#include "threads/malloc.h"
#include "threads/thread.h"
#include "kernel/hash.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "page.h"
#include "vm/swap.h"
#include "vm/frame.h"

unsigned hash_func(const struct hash_elem * elem, void * aux UNUSED) {
    struct virtual_memory_entry * page = hash_entry(elem, struct virtual_memory_entry, hash_elem);
    return hash_int((int) page->vaddr);
}

bool less_func(const struct hash_elem * elem1, const struct hash_elem * elem2, void * aux UNUSED) {
    struct virtual_memory_entry * page1 = hash_entry(elem1, struct virtual_memory_entry, hash_elem);
    struct virtual_memory_entry * page2 = hash_entry(elem2, struct virtual_memory_entry, hash_elem);
    return page1->vaddr < page2->vaddr;
}

bool virtual_memory_init(struct hash * virtual_memory) {
    return hash_init(virtual_memory,hash_func,less_func,NULL);
}

bool virtual_memory_entry_insert(struct hash * virtual_memory, struct virtual_memory_entry * entry) {
    struct hash_elem * elem = hash_insert(virtual_memory,&entry->hash_elem);
    if (elem) {
        return false;
    }
    return true;
}

bool virtual_memory_entry_delete(struct hash * virtual_memory, struct virtual_memory_entry * entry) {
    struct hash_elem * elem = hash_delete(virtual_memory,&entry->hash_elem);
    if (!elem) {
        return false;
    }
    return true;
}

struct virtual_memory_entry * virtual_memory_entry_find(void * vaddr) {
    struct virtual_memory_entry entry;
    struct hash_elem * elem;
    struct thread * cur = thread_current();

    entry.vaddr = vaddr;
    elem = hash_find(&cur->virtual_memory, &entry.hash_elem);
    if (elem != NULL) {
        return hash_entry(elem, struct virtual_memory_entry, hash_elem);
    }
    else {
        return NULL;
    }
}

void destroy_virtual_memory_entry(struct hash_elem * elem, void *aux UNUSED) {
    struct virtual_memory_entry * entry = hash_entry(elem, struct virtual_memory_entry, hash_elem);
    frame_free(entry->kpage);
    if (entry->swap_slot) {
        swap_clear(entry->swap_slot);
    }
    free(entry);
}

void virtual_memory_entry_destroy(struct hash * virtual_memory) {
    if (virtual_memory != NULL) {
        hash_destroy (virtual_memory, destroy_virtual_memory_entry);
    }
}

static bool
install_page (void *upage, void *kpage, bool writable)
{
    struct thread *t = thread_current ();

    /* Verify that there's not already a page at that virtual
       address, then map our page there. */
    return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

bool lazy_load(void * vaddr) {
    struct virtual_memory_entry * entry = virtual_memory_entry_find(vaddr);

    if (!entry) {
        return false;
    }

    struct frame * f = frame_alloc(vaddr);
    entry->kpage = f->kpage;

    switch (entry->virtual_memory)
    {
    case BINARY_FILE:
        if (file_read_at(entry->file, f->kpage, entry->read_bytes, entry->offset) != (int) entry->read_bytes) {
            frame_free(f->kpage);
            return false; 
        }
        memset (f->kpage + entry->read_bytes, 0, entry->zero_bytes);
        entry->is_loaded = true;
        break;
    case MEMORY_MAPPED_FILE:
        if (file_read_at(entry->file, f->kpage, entry->read_bytes, entry->offset) != (int) entry->read_bytes) {
            frame_free(f->kpage);
            return false;
        }
        memset (f->kpage + entry->read_bytes, 0, entry->zero_bytes);
        entry->is_loaded = true;
        break;
    case SWAPPING:
        swap_in (entry->swap_slot, f->kpage);
        entry->is_loaded = true;
        break;
    default:
        NOT_REACHED ();
    }

    /* Add the page to the process's address space. */
    if (!install_page(vaddr, f->kpage, entry->writable)) {
        frame_free(f->kpage);
        return false;
    }
  
    return true;
}

void make_free_page() {
    lock_acquire (&frame_list_lock);

    struct frame * frame = choose_frame();
    if (!frame) {
        syscall_exit(-1);
    }

    struct virtual_memory_entry * entry = virtual_memory_entry_find(frame->upage);
    if (!entry) { 
        syscall_exit(-1);
    }
 
    switch (entry->virtual_memory)
    {
        case BINARY_FILE:
        if (pagedir_is_dirty (frame->owner->pagedir, frame->upage)) {
            entry->swap_slot = swap_out(frame->kpage);
            entry->virtual_memory = SWAPPING;
        }
        break;
        case MEMORY_MAPPED_FILE:
        if (pagedir_is_dirty (frame->owner->pagedir, frame->upage)) {
            if (file_write_at (entry->file, entry->vaddr, entry->read_bytes, entry->offset) != (int) entry->read_bytes) {
                syscall_exit(-1);
            }
        }
        break;
        case SWAPPING:
        entry->swap_slot = swap_out(frame->kpage);
        break;
        default:
        syscall_exit(-1);
    }
    entry->is_loaded = false;
    frame_free(frame->kpage);
    lock_release (&frame_list_lock);
}