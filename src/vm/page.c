#include <debug.h>
#include <list.h>
#include "threads/malloc.h"
#include "threads/thread.h"
#include "vm/page.h"

/* Procedures for accessing and manipulating the supplemental page table. */

/* Add a vm_area_struct to the thread's supplemental page table. */
void spt_add(struct thread *t, struct vm_area_struct *vm_area) {
    struct list_elem *e;
    struct vm_area_struct *iter;

    ASSERT(vm_area->vm_start < vm_area->vm_end);

    /* Assert that the vm_area's virtual memory block doesn't overlap with any
       other blocks in the supplemental page table. */
    for (e = list_begin(&t->spt); e != list_end(&t->spt); e = list_next(e)) {
        iter = list_entry(e, struct vm_area_struct, elem);
        ASSERT(vm_area->vm_start > iter->vm_end || 
               vm_area->vm_end < iter->vm_start);
    }

    list_insert_ordered(&t->spt, &vm_area->elem, &spt_less, NULL);
}

/* Removes a vm_area_struct from its supplemental page table. */
void spt_remove(struct vm_area_struct *vm_area) {
    list_remove(&vm_area->elem);
    free(vm_area);
}

bool spt_less(struct list_elem *e1, struct list_elem *e2, void *aux UNUSED) {
    struct vm_area_struct *va1, *va2;

    va1 = list_entry(e1, struct vm_area_struct, elem);
    va2 = list_entry(e2, struct vm_area_struct, elem);

    return va1->vm_end <  va2->vm_start;
}
