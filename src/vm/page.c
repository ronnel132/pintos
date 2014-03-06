#include <debug.h>
#include "threads/malloc.h"
#include "threads/thread.h"
#include "vm/page.h"
#include "vm/frame.h"

/* Procedures for accessing and manipulating the supplemental page table. */

/* Add a vm_area_struct to the thread's supplemental page table. */
void spt_add(struct thread *t, struct vm_area_struct *vm_area) {
    /* Assert validity of start and end addresses. */
    ASSERT(vm_area->vm_start < vm_area->vm_end);

    /* PANIC if overlapping memory when inserting into table. */
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

/* Free the entries in T's supplemental page table. */
void spt_free(struct hash *spt) {
    struct vm_area_struct *vma;
    struct hash_iterator i;
    struct frame f;
    
    hash_first(&i, spt);
    while (hash_next(&i)) {
        vma = hash_entry(hash_cur(&i), struct vm_area_struct, elem);
        if (vma->pg_type == SWAP) {
            swap_remove(vma->swap_ind, NULL);
        }
        else if (vma->pg_type == PMEM) {
            f.kpage = vma->kpage; 
            palloc_free_page(vma->kpage);
            frame_table_remove(&f);
        }
        free(vma);
    }
}
