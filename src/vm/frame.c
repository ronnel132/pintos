#include <debug.h>
#include <hash.h>
#include "filesys/file.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/interrupt.h"

struct lock frame_lock;
struct lock filesys_lock;

/* Evicts a frame from the frame table and returns the kernel virtual address
   of that frame. */
void *frame_evict(void) {
    struct list_elem *e;
    struct frame *frame;
    struct vm_area_struct *vma; 
    int bytes_written;
    int fs_lock = 0;
    enum intr_level old_level;
    void *ret_kpage;

    lock_acquire(&frame_lock);
    ASSERT(list_size(&frame_queue) > 0);

    while (1) {
        e = list_pop_front(&frame_queue);
        frame = list_entry(e, struct frame, q_elem);

        if (!pagedir_is_accessed(frame->thread->pagedir, frame->upage)) {
            /* The frame has NOT been accessed. */
            /* Swap it out. First update the vm_area_struct for this page. */
            vma = spt_get_struct(frame->thread, frame->upage); 
            /* The vm_area_struct for this upage MUST be present in the
               supplemental page table for this thread. */
            ASSERT(vma != NULL);
            ASSERT(vma->pg_type != SWAP);
            ASSERT(vma->kpage != NULL);

            if (vma->pg_type != SWAP) {
                /* A stack page. */
                vma->kpage = NULL;
                vma->pg_type = SWAP;
                vma->swap_ind = swap_add(frame->kpage);
            }
            vma->kpage = NULL;

            pagedir_clear_page(frame->thread->pagedir, vma->vm_start);

            /* Save the kpage we return before freeing in 
               frame_table_remove. */
            ret_kpage = frame->kpage;
            /* Remove the frame from the frame table. */
            frame_table_remove(frame);

            
            /* Return the now free kernel page. */
            lock_release(&frame_lock);
            return ret_kpage;
        }
        else {
            /* The frame HAS been accessed. */
            /* Set its accessed bit to 0, then enqueue it. */
            pagedir_set_accessed(frame->thread->pagedir, frame->upage, 0);
            list_push_back(&frame_queue, &frame->q_elem);
        }
    }
    lock_release(&frame_lock);
}

/* Remove the frame from the frame table. */
void frame_table_remove(struct frame *frame) {
    struct frame *f;
    struct hash_elem *entry;
    entry = hash_delete(&frame_table, &frame->elem);
    if (entry != NULL) {
        f = hash_entry(entry, struct frame, elem);
        free(f);
    }
}

/* Add a frame to the frame table. Keep track of the upage, kpage, and the 
   process whose upage virtual address we wish to store in the kernel page. */
void frame_add(struct thread *t, void *upage, void *kpage) {
    struct frame *frame;
    ASSERT(upage != NULL);
    ASSERT(kpage != NULL);
    

    frame = (struct frame *) malloc(sizeof(struct frame));
    /* Store the thread/process that owns this upage. */
    frame->thread = t;
    frame->upage = upage; 
    frame->kpage = kpage;

    /* The frame table remains ordered by the physical address of the frame. */
    lock_acquire(&frame_lock);
    hash_insert(&frame_table, &frame->elem);
    list_push_back(&frame_queue, &frame->q_elem);
    lock_release(&frame_lock);
}

/* The function used for ordered inserts into the frame table. We order the 
   frame table list by physical address. */
bool frame_less(const struct hash_elem *elem1, const struct hash_elem *elem2, 
                void *aux UNUSED) {
    struct frame *f1, *f2;
    uintptr_t paddr1, paddr2; 

    f1 = hash_entry(elem1, struct frame, elem);
    f2 = hash_entry(elem2, struct frame, elem);

    /* Compute the physical addresses of each frame's kernel virtual
       address. */
    paddr1 = vtop(f1->kpage);
    paddr2 = vtop(f2->kpage);
   
    return paddr1 < paddr2;
}

unsigned frame_hash_func(const struct hash_elem *element, void *aux UNUSED) {
    struct frame *frame;
    frame = hash_entry(element, struct frame, elem);
    return hash_int((int) frame->kpage);
}
