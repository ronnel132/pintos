#include <string.h>
#include <stdio.h>
#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"

static void cache_evict(void);
static struct cache_entry *cache_miss(block_sector_t sector_idx);

/*! Initialize the buffer cache by malloc'ing the space it needs. */
void cache_init(void) {
    hash_init(&cache_table, &cache_hash, &cache_less, NULL);
    list_init(&cache_queue);
}

/* Pick a cache entry to evict, and kick it out of the cache 
 * Using second chance strategy
 */
static void cache_evict(void) {
    struct list_elem *e;
    struct cache_entry *c;
    // TODO: Locks
    while (1) {
        e = list_pop_front(&cache_queue);
        c = list_entry(e, struct cache_entry, q_elem);
        
        /* If accessed, clear accessed bit and push back */
        if (c->accessed) {
            c->accessed = 0;
            list_push_back(&cache_queue, &c->q_elem);
        }

        /* Otherwise remove it. Also check if writeback is needed */
        else {
            if (c->dirty) {
                block_write(fs_device, c->sector_idx, c->data); 
            }
            
            hash_delete(&cache_table, &c->elem);
            free(c->data);
            free(c);
            break;
        }
    }
    return;
}

/* Called in cache_read and cache_write when the desired block is not cached.
   Store it in the cache, then return its corresponding cache_entry struct. */
static struct cache_entry *cache_miss(block_sector_t sector_idx) {
    struct cache_entry *ce; 
    ce = malloc(sizeof(struct cache_entry));
    if (ce == NULL) {
        PANIC("Filesystem cache failure.");
    }
    if (hash_size(&cache_table) == CACHE_SIZE) {
        cache_evict();
    }
    /* TODO: synchronization: what if another process adds to the cache table
       before we do? */ 
    ce->data = malloc(BLOCK_SECTOR_SIZE);
    if (ce->data == NULL) {
        PANIC("Filesystem cache failure.");
    }
    ce->sector_idx = sector_idx;
    block_read(fs_device, sector_idx, ce->data);
    ce->accessed = 0;
    ce->dirty = 0;
    hash_insert(&cache_table, &ce->elem);
    list_push_back(&cache_queue, &ce->q_elem);
    return ce;
}


/*! Find the cached sector at SECTOR_IDX and read SIZE bytes at offset
    OFFSET. */
void cache_read(block_sector_t sector_idx, void *buffer, off_t size,
                off_t offset) {
    struct cache_entry ce; 
    struct hash_elem *helem;
    struct cache_entry *stored_ce;

    ASSERT(offset + size <= BLOCK_SECTOR_SIZE);

    ce.sector_idx = sector_idx;
    helem = hash_find(&cache_table, &ce.elem);
    stored_ce = helem == NULL ? NULL : hash_entry(helem, struct cache_entry,
                                                  elem);
    if (stored_ce == NULL) {
        stored_ce = cache_miss(sector_idx);
    }
    else {
        stored_ce->accessed = 1;
    }

    memcpy(buffer, (void *) ((uint32_t) stored_ce->data + (uint32_t) offset), 
           (size_t) size);
}

/*! Find the cached sector at SECTOR_IDX and write SIZE bytes starting at 
    OFFSET from BUFFER to the cached sector. */
void cache_write(block_sector_t sector_idx, void *buffer, off_t size,
                 off_t offset) {
    struct cache_entry ce;
    struct hash_elem *helem;
    struct cache_entry *stored_ce;

    ASSERT(offset + size <= BLOCK_SECTOR_SIZE); 

    ce.sector_idx = sector_idx;
    helem = hash_find(&cache_table, &ce.elem);
    stored_ce = helem == NULL ? NULL : hash_entry(helem, struct cache_entry,
                                                  elem);
    if (stored_ce == NULL) {
        stored_ce = cache_miss(sector_idx);
    } 
    else {
        stored_ce->accessed = 1;
    }

    memcpy((void *) ((uint32_t) stored_ce->data + (uint32_t) offset), buffer, 
           (size_t) size);
    stored_ce->dirty = 1;
}


unsigned cache_hash(const struct hash_elem *element, void *aux UNUSED) {
    struct cache_entry *ce = hash_entry(element, struct cache_entry, elem);
    return hash_int(ce->sector_idx);
}

bool cache_less(const struct hash_elem *a, const struct hash_elem *b,
                void *aux UNUSED) {
    struct cache_entry *ce1 = hash_entry(a, struct cache_entry, elem);
    struct cache_entry *ce2 = hash_entry(b, struct cache_entry, elem);
    return ce1->sector_idx < ce2->sector_idx;
}
