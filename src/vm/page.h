#ifndef PAGE_H
#define PAGE_H

#include <list.h>
#include "devices/block.h"
#include "filesys/off_t.h"
#include "threads/thread.h"

/* What type of pages are stored in this vm_area? They can be in the file 
   system, in swap, or an all zero page. */
enum pg_type_flags {
    /* Page is in the file system. */
    FILE_SYS = 001,
    /* Page is in swap space. */
    SWAP = 002,
    /* Page is all zeros. */
    ZERO = 003,
    /* Page is in physical memory. */
    PMEM = 004
};

struct vm_area_struct {
    /* Virtual memory start and end addresses. */
    void *vm_start;
    void *vm_end;

    /* The kernel virtual address for when pg_type == PMEM. */
    void *kpage; 

    /* Is this memory area writable. */
    bool writable;

    /* Is this area pinned */
    bool pinned;

    /* Pointer to the file object of the mapped file, if any. */
    struct file *vm_file;

    /* Offset in the mapped file. */
    off_t ofs;

    uint32_t pg_read_bytes;

    /* Sector index within the swap partition. NULL if pg_type != SWAP. */
    block_sector_t swap_ind; 

    /* The type of page. */
    enum pg_type_flags pg_type;

    struct hash_elem elem;
};

void spt_add(struct thread *t, struct vm_area_struct *vm_area);  
struct vm_area_struct *spt_get_struct(struct thread *t, void *upage);
void spt_remove(struct thread *t, struct vm_area_struct *vm_area);
bool spt_present(struct thread *t, void *upage);
unsigned spt_hash_func(const struct hash_elem *element, void *aux UNUSED); 
bool spt_less(struct hash_elem *e1, struct hash_elem *e2, void *aux UNUSED);
void spt_free(struct hash *spt);

#endif
