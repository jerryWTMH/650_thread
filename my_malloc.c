#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "my_malloc.h"

Blockmeta * first_free_lock = NULL;
Blockmeta * last_free_lock = NULL;
//Blockmeta * first_block = NULL;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
__thread Blockmeta * first_free_nolock = NULL;
__thread Blockmeta * last_free_nolock = NULL;

/*
void printFreeList() {
  Blockmeta * cur = first_free_block;
  while (cur != NULL) {
    printf("cur: %p, cur->size: %lu\n", cur, cur->size);
    cur = cur->next;
  }
  }*/

void * ts_malloc_lock(size_t size) {
  pthread_mutex_lock(&lock);
  int sbrk_lock = 0;
  void * p = bf_malloc(size, &first_free_lock, &last_free_lock, sbrk_lock);
  pthread_mutex_unlock(&lock);
  return p;
}
void ts_free_lock(void * ptr) {
  pthread_mutex_lock(&lock);
  bf_free(ptr, &first_free_lock, &last_free_lock);
  pthread_mutex_unlock(&lock);
}

void * ts_malloc_nolock(size_t size) {
  int sbrk_lock = 1;
  void * p = bf_malloc(size, &first_free_nolock, &last_free_nolock, sbrk_lock);
  return p;
}
void ts_free_nolock(void * ptr) {
  bf_free(ptr, &first_free_nolock, &last_free_nolock);
}

// void * reuse_block(size_t size,
//                    Blockmeta * p,
//                    Blockmeta ** first_free_block,
//                    Blockmeta ** last_free_block) {
//   if (p->size > size + META_SIZE) {
//     Blockmeta * splitted_block;
//     splitted_block = (Blockmeta *)((char *)p + META_SIZE + size);
//     splitted_block->size = p->size - size - META_SIZE;
//     splitted_block->isfree = 1;
//     splitted_block->next = NULL;
//     splitted_block->prev = NULL;

//     delete_block(p, first_free_block, last_free_block);
//     insert_block(splitted_block, first_free_block, last_free_block);
//     p->size = size;
//   }
//   else {
//     delete_block(p, first_free_block, last_free_block);
//   }
//   p->isfree = 0;
//   p->next = NULL;
//   p->prev = NULL;
//   return (char *)p + META_SIZE;
// }

void * allocate_block(size_t size, int sbrk_lock) {
  Blockmeta * new_block = NULL;
  if (sbrk_lock == 0) {
    new_block = sbrk(size + META_SIZE);
  }
  else {
    pthread_mutex_lock(&lock);
    new_block = sbrk(size + META_SIZE);
    pthread_mutex_unlock(&lock);
  }
  new_block->size = size;
  new_block->isfree = 0;
  new_block->prev = NULL;
  new_block->next = NULL;
  return (char *)new_block + META_SIZE;
}

// void insert_block(Blockmeta * p, Blockmeta ** first_free_block, Blockmeta ** last_free_block) {
//   if ((*first_free_block == NULL) || (p < *first_free_block)) {
//     p->prev = NULL;
//     p->next = *first_free_block;
//     if (p->next != NULL) {
//       p->next->prev = p;
//     }
//     else {
//       *last_free_block = p;
//     }
//     *first_free_block = p;
//   }
//   else {
//     Blockmeta * curr = *first_free_block;
//     while ((curr->next != NULL) && (p > curr->next)) {
//       curr = curr->next;  //curr< p< curr->next, keep going
//     }
//     p->prev = curr;
//     p->next = curr->next;
//     curr->next = p;
//     if (p->next != NULL) {
//       p->next->prev = p;
//     }
//     else {
//       *last_free_block = p;
//     }
//   }
// }

void insert_block(Blockmeta * blockPtr, Blockmeta ** first_free_block, Blockmeta ** last_free_block){
   if((*first_free_block == NULL) || (blockPtr < *first_free_block)){
        // block < first_free_block just give a correct sequence
        blockPtr->prev = NULL;
        blockPtr->next = *first_free_block;
        *first_free_block = blockPtr;
        if(blockPtr->next == NULL || blockPtr == *last_free_block){        
            *last_free_block = blockPtr;
        } else{
            blockPtr->next->prev = blockPtr;
        }
    } else if(blockPtr > *last_free_block){
        (*last_free_block) ->next = blockPtr;
        blockPtr->prev = *last_free_block;
        blockPtr->next = NULL;
        *last_free_block = blockPtr;
    } 
    else{
        // block should be put in the middle of the list
        Blockmeta * curr;
        int overLastOne = 1;
        for(curr = *first_free_block; curr->next != NULL; curr = curr->next){
            if(blockPtr <= curr->next){
                overLastOne = 0;
                break;
            }
        }
        blockPtr->prev = curr;
        if(overLastOne){
            blockPtr->next = NULL;
            curr-> next = blockPtr;
        } else{
            blockPtr->next = curr->next;
            curr->next = blockPtr;
            blockPtr->next->prev = blockPtr;
        }

    }
}

// void delete_block(Blockmeta * p,
//                   Blockmeta ** first_free_block,
//                   Blockmeta ** last_free_block) {
//   if ((*last_free_block == *first_free_block) && (*last_free_block == p)) {
//     *last_free_block = *first_free_block = NULL;
//   }
//   else if (*last_free_block == p) {
//     *last_free_block = p->prev;
//     (*last_free_block)->next = NULL;
//   }
//   else if (*first_free_block == p) {
//     *first_free_block = p->next;
//     (*first_free_block)->prev = NULL;
//   }
//   else {
//     p->prev->next = p->next;
//     p->next->prev = p->prev;
//   }
// }

void delete_block(Blockmeta * p,
                  Blockmeta ** first_free_block,
                  Blockmeta ** last_free_block){
    if(p == *first_free_block){
        if(*last_free_block == *first_free_block){
            *last_free_block = *first_free_block = NULL;
        } else{
            *first_free_block = p->next;
            (*first_free_block)->prev = NULL;
        }
    } else{
        if(p != *last_free_block){
            p ->prev->next= p -> next;
            p ->next ->prev = p -> prev;
        } else{
            *last_free_block = p -> prev;
            (*last_free_block) -> next = NULL;
        }
    }
}


void check_merge(Blockmeta * p, Blockmeta ** first_free_block, Blockmeta ** last_free_block){
    //if the previous space is also a free space
    int left_part_merge = 0;
    int right_part_merge = 0;
    if(p->prev){
        if((char *)p->prev + p->prev->size + META_SIZE == (char *)p){
            left_part_merge = 1;
        }
    }
    if(p->next){
        if((char *)p + p->size + META_SIZE == (char *)p->next){
            right_part_merge = 1;
        }
    }
    if(left_part_merge && right_part_merge){
       p->prev->size += META_SIZE + p->size;
       delete_block(p, first_free_block, last_free_block);
       p->size += META_SIZE + p->next->size;       
       delete_block(p->next, first_free_block, last_free_block);
    } else if(left_part_merge){
        p->prev->size += META_SIZE + p->size;
        delete_block(p, first_free_block, last_free_block);
    } else if(right_part_merge){
       p->size += META_SIZE + p->next->size;
       delete_block(p->next, first_free_block, last_free_block);
    }
}




// void bf_free(void * ptr, Blockmeta ** first_free_block, Blockmeta ** last_free_block) {
//   Blockmeta * p;
//   p = (Blockmeta *)((char *)ptr - META_SIZE);
//   p->isfree = 1;

//   insert_block(p, first_free_block, last_free_block);

//   if ((p->next != NULL) && ((char *)p + p->size + META_SIZE == (char *)p->next)) {
//     p->size += META_SIZE + p->next->size;
//     delete_block(p->next, first_free_block, last_free_block);
//     //p->next->next = NULL;
//     //p->next->prev = NULL;
//   }
//   if ((p->prev != NULL) &&
//       ((char *)p->prev + p->prev->size + META_SIZE == (char *)p)) {
//     p->prev->size += META_SIZE + p->size;
//     delete_block(p, first_free_block, last_free_block);
//     //p->next = NULL;
//     //p->prev = NULL;
//   }
// }

void bf_free(void * ptr, Blockmeta ** first_free_block, Blockmeta ** last_free_block) {
  Blockmeta * realPtr;
    realPtr = (Blockmeta *)((char *)ptr - sizeof(Blockmeta));
    // insert the block into the free block list.
    insert_block(realPtr,first_free_block,  last_free_block);
    // check whether there would be some adjacent free blocks near the block that ptr points to.
    check_merge(realPtr, first_free_block, last_free_block);
  }

// void * bf_malloc(size_t size,
//                  Blockmeta ** first_free_block,
//                  Blockmeta ** last_free_block,
//                  int sbrk_lock) {
//   Blockmeta * p = *first_free_block;
//   Blockmeta * min_ptr = NULL;
//   while (p != NULL) {
//     if (p->size > size) {
//       if ((min_ptr == NULL) || (p->size < min_ptr->size)) {
//         min_ptr = p;
//       }
//     }
//     if (p->size == size) {
//       min_ptr = p;
//       break;
//     }
//     p = p->next;
//   }
//   if (min_ptr != NULL) {
//     return reuse_block(size, min_ptr, first_free_block, last_free_block);
//   }
//   else {
//     return allocate_block(size, sbrk_lock);
//   }
// }


Blockmeta * get_sliced_block(Blockmeta * blockPtr, size_t size){
    // return a splitted block.
        Blockmeta * slicedBlock;
        slicedBlock = (Blockmeta *)((char *)blockPtr + META_SIZE + size);
        slicedBlock -> size = blockPtr->size;
        slicedBlock -> size -= (size + META_SIZE);
        slicedBlock -> next = NULL;
        slicedBlock -> prev = NULL;
        // useless
        Blockmeta * temp;
        temp = slicedBlock;
        temp->size = slicedBlock->size;
        temp -> next = NULL;
        temp -> prev = NULL;
        return temp;
}

void * bf_malloc(size_t size, Blockmeta ** first_free_block,
                 Blockmeta ** last_free_block,
                 int sbrk_lock){
        Blockmeta * p = *first_free_block;
        size_t min = __SIZE_MAX__;
        Blockmeta * min_ptr = NULL;
        while(p != NULL){
            if(p->size >= size){
                size_t diff = p->size - size;
                if(diff < min){
                    min = diff;
                    min_ptr = p;
                } else if(diff == 0){
                    min_ptr = p;
                    break;
                }
            }
            p = p->next;
        }
        if(min_ptr == NULL){
            // cannot find a suitable free block!
            return allocate_block(size, sbrk_lock);
        } else{
            // using min_ptr block
            if(min_ptr->size > size + META_SIZE){
                // slicedBlock is the memory space left after allocating.
                Blockmeta * slicedBlock = get_sliced_block(min_ptr, size);
                // wanna use block p, so we need to remove it in the list
                delete_block(min_ptr, first_free_block, last_free_block);
                // wanna add the slicedBlock into the list
                insert_block(slicedBlock, first_free_block, last_free_block);
                min_ptr->size = size;
            } else{
                delete_block(min_ptr, first_free_block, last_free_block);
            }
            min_ptr->prev = NULL;
            min_ptr->next = NULL;
            return (char *)min_ptr + META_SIZE;
        }
        
    
}