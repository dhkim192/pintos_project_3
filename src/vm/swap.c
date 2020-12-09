#include "vm/swap.h"
#include <bitmap.h>
#include "threads/synch.h"
#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/interrupt.h"
#include "userprog/syscall.h"

struct lock swapping_lock;
struct bitmap * swapping_bitmap;

extern struct lock filesys_lock;

void swap_init(size_t size) {
    lock_init(&swapping_lock);
    swapping_bitmap = bitmap_create(size);
}

void swap_in(size_t swap_slot, void *kaddr) {
    int i;

    struct block * swapping_block;
    swapping_block = block_get_role(BLOCK_SWAP);

    if (pg_ofs(kaddr)) {
        syscall_exit(-1);
    }

    if (!swap_slot) {
        syscall_exit(-1);
    }

    swap_slot--;

    lock_acquire(&filesys_lock);
    lock_acquire(&swapping_lock);

    for (i = 0; i < 8; i++) {
        block_read(swapping_block, swap_slot + i, kaddr + BLOCK_SECTOR_SIZE * i);
    }

    bitmap_set_multiple(swapping_bitmap, swap_slot, 1, false);

    if (pg_ofs(kaddr)) {
        syscall_exit(-1);
    }

    lock_release(&swapping_lock);
    lock_release(&filesys_lock);
}

size_t swap_out(void * kaddr) {
    int i;

    struct block * swapping_block;
    swapping_block = block_get_role(BLOCK_SWAP);

    lock_acquire(&filesys_lock);
    lock_acquire(&swapping_lock);

    if (pg_ofs(kaddr)) {
        syscall_exit(-1);
    }

    size_t swap_slot;
    swap_slot = bitmap_scan_and_flip(swapping_bitmap, 0, 1, false);
    if (BITMAP_ERROR == swap_slot) {
        return BITMAP_ERROR;
    }

    for (i = 0; i < 8; i++) {
        block_write(swapping_block, swap_slot + i, kaddr + BLOCK_SECTOR_SIZE * i);
    }

    if (pg_ofs(kaddr)) {
        syscall_exit(-1);
    }
  
    lock_release(&swapping_lock);
    lock_release(&filesys_lock);

    swap_slot++;
    return swap_slot;
}

void swap_clear(size_t swap_slot) {
    if (!swap_slot) {
        return;
    }
    swap_slot--;
    lock_acquire(&swapping_lock);
    bitmap_set_multiple(swapping_bitmap, swap_slot, 1, false);
    lock_release(&swapping_lock);
}