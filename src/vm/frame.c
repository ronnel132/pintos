#include <debug.h>
#include "vm/frame.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

/* Evicts a frame from the frame table and returns the kernel virtual address
   of that frame. */
void *frame_evict(void) {
    /* TODO: Implement fully. */
    ASSERT(0);
}

/* Add a frame to the frame table. Keep track of the upage, kpage, and the 
   process whose upage virtual address we wish to store in the kernel page. */
void frame_add(pid_t pid, void *upage, void *kpage) {
    struct frame *frame;
    ASSERT(upage != NULL);
    ASSERT(kpage != NULL);

    frame = (struct frame *) malloc(sizeof(struct frame));
    frame->pid = pid;
    frame->upage = upage; 
    frame->kpage = kpage;

    /* The frame table remains ordered by the physical address of the frame. */
    list_insert_ordered(&frame_table, &frame->elem, &frame_less, NULL);
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
