#include <debug.h>
#include <stdio.h>

#include "vm/swap.h"
#include "devices/block.h"
#include "threads/malloc.h"
#include "threads/synch.h"

struct block *swap_device;

/* Lock for accessing the global swap table. */
struct lock swap_lock;

/*! Initialize the swap device. */
void swap_init(void) {
    swap_device = block_get_role(BLOCK_SWAP);
    lock_init(&swap_lock);
    if (swap_device == NULL) {
        PANIC("No swap device found, can't initialize the swap partition.");
    }

    /* Initialize the swap hash table. */
    if (!hash_init(&swap_table, &swap_hash_func, &swap_hash_less, NULL)) {
        PANIC("Unable to initialize swap partition hash table.");
    }
}

/* Store the page at KPAGE into the swap, and record this in the swap table. */
block_sector_t swap_add(void *kpage) {
    struct swap_slot ss_iter;
    struct swap_slot *ss;
    block_sector_t i, j, swap_size;
    bool found_space = false;

    swap_size = block_size(swap_device);

    /* Insert into the swap table. */
    lock_acquire(&swap_lock);

    /* Find the first free sector within the swap partition to store the KPAGE
       contents. */
    for (i = 0; i < swap_size; i += SECTORS_PER_PAGE) {
        ss_iter.sector_ind = i;
        if (hash_find(&swap_table, &ss_iter.hash_elem) == NULL) {
             found_space = true;
             /* Write to sectors i to i + SECTORS_PER_PAGE - 1, since each 
                sector is only 512 bytes in size. */
             for (j = 0; j < SECTORS_PER_PAGE; j++) {
                 block_write(swap_device, i + j, 
                             kpage + BLOCK_SECTOR_SIZE * j); 
             }
             break;
        }
    }
    if (!found_space) {
        PANIC("Swap partition full.");
    }

    /* Create a swap_slot entry. */
    ss = (struct swap_slot *) malloc(sizeof(struct swap_slot));
    ss->sector_ind = i;
    if (hash_insert(&swap_table, &ss->hash_elem) != NULL) {
        PANIC("Overwriting swap partition write detected.");
    }
    lock_release(&swap_lock);
    return i; 
}

/* Remove the swapped in page at SECTOR, and write it to BUFFER.
   If BUFFER is NULL, remove from swap table and do NOT perform write. */
void swap_remove(block_sector_t sector, void *buffer) {
    struct hash_elem *e;
    struct swap_slot ss;
    block_sector_t i;
    if (buffer != NULL) {
        for (i = 0; i < SECTORS_PER_PAGE; i++) {
            block_read(swap_device, sector + i, buffer + BLOCK_SECTOR_SIZE * i);
        }
    }
    /* Remove the struct swap_slot from the swap table. */ 
    ss.sector_ind = sector; 
    lock_acquire(&swap_lock);
    e = hash_delete(&swap_table, &ss.hash_elem);
    if (e == NULL) {
        PANIC("Attempting to release a free slot in swap.");
    }
    lock_release(&swap_lock);
    free(hash_entry(e, struct swap_slot, hash_elem));
}

unsigned swap_hash_func(const struct hash_elem *element, void *aux UNUSED) {
    /* Temp variable for hash computation. */
    struct swap_slot *ss;

    ss = hash_entry(element, struct swap_slot, hash_elem);
    
    /* The sector_ind is a unique identifier within the swap table. */
    return hash_int((int) ss->sector_ind);
}

/* Less function used to compare hash elements */
bool swap_hash_less(const struct hash_elem *a, const struct hash_elem *b,
                    void *aux UNUSED) {
    struct swap_slot *ssa, *ssb; 
    ssa = hash_entry(a, struct swap_slot, hash_elem);
    ssb = hash_entry(b, struct swap_slot, hash_elem);
    return ssa->sector_ind < ssb->sector_ind;
}
