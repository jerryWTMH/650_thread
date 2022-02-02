#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
struct blockmeta{
    size_t size;
    bool isfree;
    struct blockmeta * next;
    struct blockmeta * prev;
};
typedef struct blockmeta Blockmeta;
#define META_SIZE sizeof(Blockmeta)

// Hw default methods
void * ff_malloc(size_t size);
void ff_free(void * ptr);
void * bf_malloc(size_t size, Blockmeta **firstFreeBlock, Blockmeta ** lastFreeBlock, int sbrk_lock);
void bf_free(void * ptr, Blockmeta ** firstFreeBlock, Blockmeta **lastFreeBlock);

// Jerry's methods
void * use_existing_block(size_t size, Blockmeta * p);
void * allocate_block(size_t size, int sbrk_lock);
void insert_block(Blockmeta * p, Blockmeta ** firstFreeBlock, Blockmeta ** lastFreeBlock);
void delete_block(Blockmeta * p,  Blockmeta ** firstFreeBlock, Blockmeta ** lastFreeBlock);
Blockmeta * get_sliced_block(Blockmeta * blockPtr, size_t size);
void check_merge(Blockmeta * p, Blockmeta ** firstFreeBlock, Blockmeta ** lastFreeBlock);
