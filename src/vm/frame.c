#include <debug.h>
#include "filesys/file.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

struct lock frame_lock;

/* Evicts a frame from the frame table and returns the kernel virtual address
   of that frame. */
void *frame_evict(void) {
    struct list_elem *e;
    struct frame *frame;
    struct vm_area_struct *vma; 
    int bytes_written;

    ASSERT(list_size(&frame_queue) > 0);
    lock_acquire(&frame_lock);
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

            /* If it's a file don't swap; write back */
            if (vma->pg_type == FILE_SYS) {
                if (pagedir_is_dirty(frame->thread->pagedir, frame->upage)) {
                    ASSERT(vma->writable);
                    file_seek(vma->vm_file, vma->ofs);
                    bytes_written = file_write(vma->vm_file,
                                               vma->vm_start,
                                               vma->pg_read_bytes);
                    ASSERT(bytes_written == vma->pg_read_bytes);
                }
            }
            else if (vma->pg_type == PMEM) {
                /* A stack page. */
                vma->kpage = NULL;
                vma->pg_type = SWAP;
                vma->swap_ind = swap_add(frame->kpage);
            }
            vma->kpage = NULL;

            pagedir_clear_page(frame->thread->pagedir, vma->vm_start);

            /* Remove the frame from the frame table. */
            frame_table_remove(frame);

            lock_release(&frame_lock);
            
            /* Return the now free kernel page. */
            return frame->kpage;
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
    list_remove(&frame->elem); 
    free(frame);
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
    list_insert_ordered(&frame_table, &frame->elem, &frame_less, NULL);
    list_push_back(&frame_queue, &frame->q_elem);
    lock_release(&frame_lock);
}

/* The function used for ordered inserts into the frame table. We order the 
   frame table list by physical address. */
bool frame_less(const struct list_elem *elem1, const struct list_elem *elem2, 
                void *aux UNUSED) {
    struct frame *f1, *f2;
    uintptr_t paddr1, paddr2; 

    f1 = list_entry(elem1, struct frame, elem);
    f2 = list_entry(elem2, struct frame, elem);

    /* Compute the physical addresses of each frame's kernel virtual
       address. */
    paddr1 = vtop(f1->kpage);
    paddr2 = vtop(f2->kpage);
   
    return paddr1 <= paddr2;
}
