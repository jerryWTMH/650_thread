#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

struct blockmeta {
  size_t size;
  int isfree;
  // size_t pre_size;
  struct blockmeta * next;
  struct blockmeta * prev;
};
typedef struct blockmeta Blockmeta;
#define META_SIZE sizeof(Blockmeta)
//void printFreeList();
void * reuse_block(size_t size,
                   Blockmeta * p,
                   Blockmeta ** first_free_block,
                   Blockmeta ** last_free_block);
void * allocate_block(size_t size, int sbrk_lock);
void insert_block(Blockmeta * p, Blockmeta ** first_free_block, Blockmeta ** last_free_block);
void delete_block(Blockmeta * p,
                  Blockmeta ** first_free_block,
                  Blockmeta ** last_free_block);
Blockmeta * get_sliced_block(Blockmeta * blockPtr, size_t size);
void * bf_malloc(size_t size,
                 Blockmeta ** first_free_block,
                 Blockmeta ** last_free_block,
                 int sbrk_lock);
void bf_free(void * ptr, Blockmeta ** first_free_block, Blockmeta ** last_free_block);
void check_merge(Blockmeta * p, Blockmeta ** first_free_block, Blockmeta ** last_free_block);


void * ts_malloc_lock(size_t size);
void ts_free_lock(void * ptr);
void * ts_malloc_nolock(size_t size);
void ts_free_nolock(void * ptr);