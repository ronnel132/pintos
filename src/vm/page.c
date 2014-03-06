#include <debug.h>
#include "threads/malloc.h"
#include "threads/thread.h"
#include "vm/page.h"

/* Procedures for accessing and manipulating the supplemental page table. */

/* Add a vm_area_struct to the thread's supplemental page table. */
void spt_add(struct thread *t, struct vm_area_struct *vm_area) {
    ASSERT(vm_area->vm_start < vm_area->vm_end);

    /* FOR DEBUGGING PURPOSES. */
    /* Assert that the vm_area's virtual memory block doesn't overlap with any
       other blocks in the supplemental page table. */
//     for (e = list_begin(&t->spt); e != list_end(&t->spt); e = list_next(e)) {
//         iter = list_entry(e, struct vm_area_struct, elem);
//         ASSERT(vm_area->vm_start > iter->vm_end || 
//                vm_area->vm_end < iter->vm_start);
//     }

    if (hash_insert(&t->spt, &vm_area->elem) != NULL) {
        PANIC("Overlapping vm_area_structs detected.");
    }
}

/* Returns the vm_area_struct in thread T corresponding to UPAGE. If not 
   present, return NULL. */
struct vm_area_struct *spt_get_struct(struct thread *t, void *upage) {
    struct hash_elem *h_elem;
    struct vm_area_struct vma;

    vma.vm_start = upage;
    
    h_elem = hash_find(&t->spt, &vma.elem);
    return h_elem == NULL ? NULL : 
           hash_entry(h_elem, struct vm_area_struct, elem);
}

/* Removes a vm_area_struct from its supplemental page table. */
void spt_remove(struct thread *t, struct vm_area_struct *vm_area) {
    hash_delete(&t->spt, &vm_area->elem);
    free(vm_area);
}

/* Returns TRUE if an entry for UPAGE is present in the supplemental page
   table for this thread, FALSE otherwise. */
bool spt_present(struct thread *t, void *upage) {
    struct vm_area_struct vma;
    vma.vm_start = upage;
    return hash_find(&t->spt, &vma.elem) != NULL;
}

unsigned spt_hash_func(const struct hash_elem *element, void *aux UNUSED) {
    struct vm_area_struct *vma;
    vma = hash_entry(element, struct vm_area_struct, elem);
    return hash_int((int) vma->vm_start);
}

bool spt_less(struct hash_elem *e1, struct hash_elem *e2, void *aux UNUSED) {
    struct vm_area_struct *va1, *va2;

    va1 = hash_entry(e1, struct vm_area_struct, elem);
    va2 = hash_entry(e2, struct vm_area_struct, elem);

    return va1->vm_start <  va2->vm_start;
}
