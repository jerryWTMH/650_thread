#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
struct blockmeta{
    size_t size;
    struct blockmeta * next;
    struct blockmeta * prev;
};
typedef struct blockmeta Blockmeta;
#define META_SIZE sizeof(Blockmeta)

// Hw default methods
void * ff_malloc(size_t size);
void ff_free(void * ptr);
void * bf_malloc(size_t size);
void bf_free(void * ptr);
unsigned long get_data_segment_size();
unsigned long get_data_segment_free_space_size();

// Jerry's methods
void * use_existing_block(size_t size, Blockmeta * p);
void * allocate_block(size_t size);
 void insert_block(Blockmeta * p);
void insert_block2(Blockmeta * p, Blockmeta * t);
void delete_block(Blockmeta * p);
Blockmeta * get_sliced_block(Blockmeta * blockPtr, size_t size);
void check_merge(Blockmeta * p);

