#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include "threads/thread.h"
#include "vm/page.h"

struct frame {
    void * kpage;
    void * upage;
    struct thread * owner;
    struct list_elem list_elem;
};

void frame_init (void);

struct frame *frame_alloc (void *);
void frame_free (void *);


#endif /* vm/frame.h */