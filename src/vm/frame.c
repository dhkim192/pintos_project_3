#include "vm/frame.h"
#include <list.h>
#include <debug.h>
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "vm/page.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "userprog/syscall.h"
#include "vm/swap.h"

static struct list frame_list;
struct lock frame_list_lock;
struct list_elem * lru_clock;

void frame_init (void) {
    list_init(&frame_list);
    lock_init(&frame_list_lock);
    lru_clock = NULL;
}

struct frame * frame_alloc (void *upage) {
    struct frame *f = malloc(sizeof (struct frame));
    f->kpage = palloc_get_page(PAL_USER);
    if (!f->kpage) {
        make_free_page();
        f->kpage = palloc_get_page(PAL_USER);
    }
    
    f->owner = thread_current();
    f->upage = upage;
  
    list_push_back(&frame_list, &f->list_elem);
    return f;
}

void frame_delete(struct frame * frame) {
    if (lru_clock == &frame->list_elem) {
        lru_clock = list_remove(lru_clock);
    } else {
        list_remove(&frame->list_elem);
    }
}


void frame_free (void *kpage) {
    struct list_elem * elem = NULL;
    for (elem = list_begin(&frame_list); elem != list_end(&frame_list);) {
        struct frame * temp_frame = list_entry (elem, struct frame, list_elem);
        if (temp_frame->kpage == kpage) {
            pagedir_clear_page (temp_frame->owner->pagedir, temp_frame->upage);
            frame_delete(temp_frame);
            palloc_free_page(temp_frame->kpage);
            free(temp_frame);
            break;
        } else {
            elem = list_next(elem);
        }
    }
}

struct list_elem * get_next_lru_clock() {
    if (!lru_clock || lru_clock == list_end(&frame_list)) {
        if (list_empty(&frame_list)) {
            return NULL;
        } else {
            lru_clock = list_begin(&frame_list);
            return lru_clock;
        }
    }
    lru_clock = list_next(lru_clock);
    if (lru_clock == list_end(&frame_list)) {
        lru_clock = list_begin(&frame_list);
    }
    return lru_clock;
}

struct frame * choose_frame() {
    struct frame * frame;
    struct list_elem * elem;

    elem = get_next_lru_clock();
    if (!elem) {
        syscall_exit(-1);
    }
    frame = list_entry(elem, struct frame, list_elem);
    if (!frame) {
        syscall_exit(-1);
    }

    while (pagedir_is_accessed (frame->owner->pagedir, frame->upage)){
        pagedir_set_accessed (frame->owner->pagedir, frame->upage, false);
        elem = get_next_lru_clock();
        if (!elem) {
            syscall_exit(-1);
        }
        frame = list_entry (elem, struct frame, list_elem);
        if (!frame) {
            syscall_exit(-1);
        }
    }

    return frame;
}