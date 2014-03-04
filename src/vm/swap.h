#ifndef SWAP_H
#define SWAP_H

#include <hash.h>
#include "devices/block.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

#define SECTORS_PER_PAGE (PGSIZE/BLOCK_SECTOR_SIZE)

/*! Swap device that contains the swap partition. */
struct block *swap_device;

/*! Store the swap table as a hash table. */
struct hash swap_table;

/*! An entry in the swap table. */
struct swap_slot {
    /* The sector index of the page stored in swap. */
    block_sector_t sector_ind; 

    struct hash_elem hash_elem;
};

void swap_init(void);
block_sector_t swap_add(void *kpage);
void swap_remove(block_sector_t sector, void *buffer);
unsigned swap_hash_func(const struct hash_elem *element, void *aux UNUSED);
bool swap_hash_less(const struct hash_elem *a, const struct hash_elem *b,
                    void *aux);
#endif
