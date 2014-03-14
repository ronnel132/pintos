#include <string.h>
#include <stdio.h>
#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"

static void cache_evict(void);
static struct cache_entry *cache_miss(block_sector_t sector_idx);

/*! Initialize the buffer cache by malloc'ing the space it needs. */
void cache_init(void) {
    int i;
    cache = (struct cache_block *) malloc(CACHE_SIZE * 
                                         sizeof(struct cache_block));
    if (cache == NULL) {
        PANIC("Unable to initialize file system cache.");
    }
    for (i = 0; i < CACHE_SIZE; i++) {
        cache[i].sector_idx = NULL;
        cache[i].valid = 0;
        cache[i].accessed = 0;
        cache[i].dirty = 0;
    }
    hash_init(&cache_table, &cache_hash, &cache_less, NULL);
    hand = 0;
}

/* Pick a cache entry to evict, and kick it out of the cache 
 * Using second chance strategy
 */
static void cache_evict(void) {
    struct cache_block *cb;
    struct cache_entry ce;
    struct hash_elem *elem; 
        
    while (1) {
        cb = &cache[hand];
        ASSERT(cb->valid);
        if (cb->accessed) {
            cb->accessed = 0;
            hand = (hand + 1) % CACHE_SIZE;
        }
        else {
            cb->valid = 0;
            ce.sector_idx = cb->sector_idx;
            elem = hash_find(&cache_table, &ce.elem);
            /* The cache_entry corresponding to the sector in this cache index
               should be present in the cache_table. */
            ASSERT(elem != NULL);
            hash_delete(&cache_table, elem);

            if (cb->dirty) {
                block_write(fs_device, cb->sector_idx, cb->data);
            }
            
            free(hash_entry(elem, struct cache_entry, elem));
            break;
        }
    }
    return;
}

/* Called in cache_read and cache_write when the desired block is not cached.
   Store it in the cache, then return its corresponding cache_entry struct. */
static struct cache_entry *cache_miss(block_sector_t sector_idx) {
    struct cache_entry *ce; 
    struct cache_block *cblock;

    ce = malloc(sizeof(struct cache_entry));
    if (ce == NULL) {
        PANIC("Filesystem cache failure.");
    }
    if (hash_size(&cache_table) == CACHE_SIZE) {
        cache_evict();
    }
    ASSERT(!cache[hand].valid);
    ce->cache_idx = hand; 
    hand = (hand + 1) % CACHE_SIZE;
    ce->sector_idx = sector_idx;  
    hash_insert(&cache_table, &ce->elem);

    cblock = &cache[ce->cache_idx];
    cblock->sector_idx = sector_idx;
    cblock->accessed = 0;
    cblock->dirty = 0;
    cblock->valid = 1;
    block_read(fs_device, sector_idx, cblock->data);
    return ce;
}


/*! Find the cached sector at SECTOR_IDX and read SIZE bytes at offset
    OFFSET. */
void cache_read(block_sector_t sector_idx, void *buffer, off_t size,
                off_t offset) {
    struct cache_entry ce; 
    struct hash_elem *e;
    struct cache_entry *stored_ce;
    struct cache_block *cblock;

    ASSERT(offset + size <= BLOCK_SECTOR_SIZE);

    ce.sector_idx = sector_idx;
    e = hash_find(&cache_table, &ce.elem);
    stored_ce = e == NULL ? NULL : hash_entry(e, struct cache_entry,
                                                  elem);
    bool present = stored_ce != NULL;

    if (!present) {
        stored_ce = cache_miss(sector_idx);
    }
    
    cblock = &cache[stored_ce->cache_idx];
    
    if (present) {
        cblock->accessed = 1;
    }

    memcpy(buffer, (void *) ((off_t) cblock->data + offset), 
           (size_t) size);
}

/*! Find the cached sector at SECTOR_IDX and write SIZE bytes starting at 
    OFFSET from BUFFER to the cached sector. */
void cache_write(block_sector_t sector_idx, void *buffer, off_t size,
                 off_t offset) {
    struct cache_entry ce;
    struct hash_elem *e;
    struct cache_entry *stored_ce;
    struct cache_block *cblock;

    ASSERT(offset + size <= BLOCK_SECTOR_SIZE); 

    ce.sector_idx = sector_idx;
    e = hash_find(&cache_table, &ce.elem);
    stored_ce = e == NULL ? NULL : hash_entry(e, struct cache_entry,
                                                  elem);
    bool present = stored_ce != NULL;

    if (!present) {
        stored_ce = cache_miss(sector_idx);
    }
    
    cblock = &cache[stored_ce->cache_idx];
    
    if (present) {
        cblock->accessed = 1;
    }

    memcpy((void *) ((off_t) cblock->data + offset), buffer, 
           (size_t) size);
    cblock->dirty = 1;
}


unsigned cache_hash(const struct hash_elem *element, void *aux UNUSED) {
    struct cache_entry *ce = hash_entry(element, struct cache_entry, elem);
    return hash_int((int) ce->sector_idx);
}

bool cache_less(const struct hash_elem *a, const struct hash_elem *b,
                void *aux UNUSED) {
    struct cache_entry *ce1 = hash_entry(a, struct cache_entry, elem);
    struct cache_entry *ce2 = hash_entry(b, struct cache_entry, elem);
    return ce1->sector_idx < ce2->sector_idx;
}
