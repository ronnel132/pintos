#include <debug.h>
#include "vm/frame.h"
#include "vm/swap.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

struct lock frame_lock;

/* Evicts a frame from the frame table and returns the kernel virtual address
   of that frame. */
void *frame_evict(void) {
    struct list_elem *e;
    struct frame *frame;
    static uint32_t pte;
    static uint32_t *pt, *pde;
    struct vm_area_struct *vma; 

    ASSERT(list_size(&frame_queue) > 0);
    while (1) {
        lock_acquire(&frame_lock);
        e = list_pop_front(&frame_queue);
        frame = list_entry(e, struct frame, q_elem);
        pde = frame->thread->pagedir + pd_no(frame->upage);
        /* Assert this because everything in our frame table MUST be mapped to 
           the page table entry. */
        ASSERT(*pde != 0);
        pt = pde_get_pt(*pde);

        pte = pt[pt_no(frame->upage)]; 
        if ((pte & PTE_A) == 0) {
            /* The frame has NOT been accessed. */
            /* Swap it out. First update the vm_area_struct for this page. */
            vma = spt_get_struct(frame->thread, frame->upage); 
            /* The vm_area_struct for this upage MUST be present in the
               supplemental page table for this thread. */
            ASSERT(vma != NULL);
            vma->kpage = NULL;
            vma->pg_type = SWAP;
            vma->swap_ind = swap_add(frame->kpage);
            
            /* Remove the frame from the frame table. */
            frame_table_remove(frame);
            
            lock_release(&frame_lock);
            /* Return the now free kernel page. */
            return frame->kpage;
        }
        else {
            /* The frame HAS been accessed. */
            /* Set its accessed bit to 0, then enqueue it. */
            pt[pt_no(frame->upage)] &= ~PTE_A;
            list_push_back(&frame_queue, &frame->q_elem);
            lock_release(&frame_lock);
        }
    }
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
