#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "my_malloc.h"
#include <pthread.h>
#include <stdbool.h>

Blockmeta * firstFreeLock = NULL;
Blockmeta * lastFreeLock = NULL;
//Blockmeta * first_block = NULL;
Blockmeta * testingptr = NULL;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
__thread Blockmeta * firstFreeNoLock = NULL;
__thread Blockmeta * lastFreeNoLock = NULL;

void insert_block(Blockmeta * blockPtr, Blockmeta ** firstFreeBlock, Blockmeta ** lastFreeBlock){
   if((*firstFreeBlock == NULL) || (blockPtr < *firstFreeBlock)){
        // block < firstFreeBlock just give a correct sequence
        blockPtr->prev = NULL;
        blockPtr->next = *firstFreeBlock;
        *firstFreeBlock = blockPtr;
        if(blockPtr->next == NULL || blockPtr == *lastFreeBlock){        
            *lastFreeBlock = blockPtr;
        } else{
            blockPtr->next->prev = blockPtr;
        }
    } else if(blockPtr > *lastFreeBlock){
        (*lastFreeBlock) ->next = blockPtr;
        blockPtr->prev = *lastFreeBlock;
        blockPtr->next = NULL;
        *lastFreeBlock = blockPtr;
    } 
    else{
        // block should be put in the middle of the list
        Blockmeta * curr;
        bool overLastOne = true;
        for(curr = *firstFreeBlock; curr->next != NULL; curr = curr->next){
            if(blockPtr <= curr->next){
                overLastOne = false;
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


void delete_block(Blockmeta * ptr,
                  Blockmeta ** firstFreeBlock,
                  Blockmeta ** lastFreeBlock){
    if(ptr == *firstFreeBlock){
        if(*lastFreeBlock == *firstFreeBlock){
            *lastFreeBlock = *firstFreeBlock = NULL;
        } else{
            *firstFreeBlock = ptr->next;
            (*firstFreeBlock)->prev = NULL;
        }
    } else{
        if(ptr != *lastFreeBlock){
            ptr ->prev->next= ptr -> next;
            ptr ->next ->prev = ptr -> prev;
        } else{
            *lastFreeBlock = ptr -> prev;
            (*lastFreeBlock) -> next = NULL;
        }
    }
}

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
  new_block->prev = NULL;
  new_block->next = NULL;
  return (char *)new_block + META_SIZE;
}



void check_merge(Blockmeta * p, Blockmeta ** firstFreeBlock, Blockmeta ** lastFreeBlock){
    //if the previous space is also a free space
    bool left_part_merge = false;
    bool right_part_merge = false;
    if(p->prev){
        if((char *)p->prev + p->prev->size + META_SIZE == (char *)p){
            left_part_merge = true;
        }
    }
    if(p->next){
        if((char *)p + p->size + META_SIZE == (char *)p->next){
            right_part_merge = true;
        }
    }
    if(left_part_merge && right_part_merge){
       p->prev->size += META_SIZE + p->size;
       delete_block(p, firstFreeBlock, lastFreeBlock);
       p->size += META_SIZE + p->next->size;       
       delete_block(p->next, firstFreeBlock, lastFreeBlock);
    } else if(left_part_merge){
        p->prev->size += META_SIZE + p->size;
        delete_block(p, firstFreeBlock, lastFreeBlock);
    } else if(right_part_merge){
       p->size += META_SIZE + p->next->size;
       delete_block(p->next, firstFreeBlock, lastFreeBlock);
    }
}

void * bf_malloc(size_t size, Blockmeta ** firstFreeBlock,
                 Blockmeta ** lastFreeBlock,
                 int sbrk_lock){
        Blockmeta * p = *firstFreeBlock;
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
                delete_block(min_ptr, firstFreeBlock, lastFreeBlock);
                // wanna add the slicedBlock into the list
                insert_block(slicedBlock, firstFreeBlock, lastFreeBlock);
                min_ptr->size = size;
            } else{
                delete_block(min_ptr, firstFreeBlock, lastFreeBlock);
            }
            min_ptr->prev = NULL;
            min_ptr->next = NULL;
            return (char *)min_ptr + META_SIZE;
        }
}

void bf_free(void * ptr, Blockmeta ** firstFreeBlock, Blockmeta ** lastFreeBlock) {
  Blockmeta * realPtr;
    realPtr = (Blockmeta *)((char *)ptr - sizeof(Blockmeta));
    // insert the block into the free block list.
    insert_block(realPtr,firstFreeBlock,  lastFreeBlock);
    // check whether there would be some adjacent free blocks near the block that ptr points to.
    check_merge(realPtr, firstFreeBlock, lastFreeBlock);
  }

void * ts_malloc_lock(size_t size) {
  pthread_mutex_lock(&lock);
  int sbrk_lock = 0;
  void * p = bf_malloc(size, &firstFreeLock, &lastFreeLock, sbrk_lock);
  pthread_mutex_unlock(&lock);
  return p;
}
void ts_free_lock(void * ptr) {
  pthread_mutex_lock(&lock);
  bf_free(ptr, &firstFreeLock, &lastFreeLock);
  pthread_mutex_unlock(&lock);
}

void * ts_malloc_nolock(size_t size) {    
  int sbrk_lock = 1;
  void * p = bf_malloc(size, &firstFreeNoLock, &lastFreeNoLock, sbrk_lock);
  return p;
}
void ts_free_nolock(void * ptr) {
  bf_free(ptr, &firstFreeNoLock, &lastFreeNoLock);
}

