#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "my_malloc.h"
#include <pthread.h>
#include <stdbool.h>

Blockmeta * testingptr = NULL;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
Blockmeta * firstFreeLock = NULL;
__thread Blockmeta * firstFreeNoLock = NULL;



void insert_block(Blockmeta * blockPtr, Blockmeta ** firstFreeBlock){ //finish
    if(*firstFreeBlock == NULL || (blockPtr < *firstFreeBlock)){
        // block < firstFreeBlock just give a correct sequence
        if(*firstFreeBlock == NULL){
            blockPtr->prev = NULL;
            blockPtr->next = NULL; 
            *firstFreeBlock = blockPtr;
        } else{
            blockPtr->next = *firstFreeBlock;
            blockPtr->next->prev = blockPtr;
            blockPtr->prev = NULL;
            *firstFreeBlock = blockPtr;
        }
        
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

void delete_block(Blockmeta * blockPtr,Blockmeta ** firstFreeBlock){ // finish
    if(*firstFreeBlock == blockPtr){
        if((*firstFreeBlock)->next != NULL){
            // remove the first one in an array
            blockPtr->next->prev = NULL;
            *firstFreeBlock = blockPtr->next;
        } else{
            // remove the only one item in an array
            *firstFreeBlock = NULL;
        }
    } else{
        if(blockPtr->next != NULL){
            // remove the item which is in the middle
             blockPtr ->prev->next= blockPtr -> next;
            blockPtr ->next ->prev = blockPtr -> prev;
        } else{
            // remove the last item in an array
            blockPtr->prev->next = NULL;
        }
           

    }
    
 }

Blockmeta * get_sliced_block(Blockmeta * blockPtr, size_t size){
    // return a splitted block.
        Blockmeta * slicedBlock;
        slicedBlock = (Blockmeta *)((char *)blockPtr + META_SIZE + size);
        slicedBlock -> size = blockPtr->size - size - META_SIZE;
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

void * allocate_block(size_t size, int sbrk_lock){
    Blockmeta * new_block = NULL;
    if (sbrk_lock == 0) {
        new_block = sbrk(size + META_SIZE);
    }
    else {
    pthread_mutex_lock(&lock);
    new_block = sbrk(size + META_SIZE);
    pthread_mutex_unlock(&lock);
    }
    new_block -> size = size;    
    new_block -> prev = NULL;
    new_block -> next = NULL;   
    return (char*)new_block + META_SIZE;
}

void check_merge(Blockmeta * p, Blockmeta ** firstFreeBlock){
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
       delete_block(p, firstFreeBlock);
       p->size += META_SIZE + p->next->size;       
       delete_block(p->next, firstFreeBlock);
    } else if(left_part_merge){
        p->prev->size += META_SIZE + p->size;
        delete_block(p, firstFreeBlock);
    } else if(right_part_merge){
       p->size += META_SIZE + p->next->size;
       delete_block(p->next, firstFreeBlock);
    }
    
}

void * bf_malloc(size_t size, Blockmeta ** firstFreeBlock, int sbrk_lock){
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
                delete_block(min_ptr, firstFreeBlock);
                // wanna add the slicedBlock into the list
                insert_block(slicedBlock, firstFreeBlock);
                min_ptr->size = size;
                // totalFreeSize -= (size + META_SIZE);
            } else{
                delete_block(min_ptr, firstFreeBlock);
                // totalFreeSize -= (META_SIZE + min_ptr->size); 
            }
            min_ptr->prev = NULL;
            min_ptr->next = NULL;
            return (char *)min_ptr + META_SIZE;
        }
        
    
}

void bf_free(void * ptr, Blockmeta ** firstFreeBlock){
    // The bf_free is the same as the ff_free
    Blockmeta * realPtr;
    realPtr = (Blockmeta *)((char *)ptr - META_SIZE);
    // totalFreeSize += realPtr->size + META_SIZE;
    // insert the block into the free block list.
    
    insert_block(realPtr, firstFreeBlock);
    
    Blockmeta* prev_block = *firstFreeBlock;
    check_merge(realPtr, firstFreeBlock);
}

void * ts_malloc_lock(size_t size) {
  pthread_mutex_lock(&lock);
  int sbrk_lock = 0;
  void * p = bf_malloc(size, &firstFreeLock, sbrk_lock);
  pthread_mutex_unlock(&lock);
  return p;
}

void ts_free_lock(void * ptr) {
  pthread_mutex_lock(&lock);
  bf_free(ptr, &firstFreeLock);
  pthread_mutex_unlock(&lock);
}

void * ts_malloc_nolock(size_t size) {    
  int sbrk_lock = 1;
  void * p = bf_malloc(size, &firstFreeNoLock,sbrk_lock);
  return p;
}

void ts_free_nolock(void * ptr) {
  bf_free(ptr, &firstFreeNoLock);
}