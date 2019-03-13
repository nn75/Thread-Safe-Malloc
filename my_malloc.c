#include "my_malloc.h"
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

__thread block_t * local_free_head = NULL;
__thread block_t * local_free_tail = NULL;

block_t * global_free_head = NULL;      //Global head of free list.
block_t * global_free_tail = NULL;      //Global tail of free list.

block_t * first_blk = NULL;

unsigned long data_seg_size = 0;

unsigned long get_data_segment_size(){
  return data_seg_size;
}

block_t * bf_find_block(block_t ** free_head,  size_t size){
    block_t * curr = *free_head;
    if(*free_head == NULL){
        return NULL;
    }
    block_t * bestfit = NULL;
    while(curr != NULL){
        if(curr->size == size){      
            return curr;
        }
        if(curr->size > size){
            if(bestfit == NULL || bestfit->size > curr->size){
                bestfit = curr;
            }
        }
        curr = curr->next;
    }
    return bestfit;
}


block_t * addto_sorted_free_list(block_t ** free_head, block_t ** free_tail, block_t * new_blk){
    block_t * curr = * free_head;
    if(curr == NULL){
        * free_head = new_blk;
        * free_tail = new_blk;
        new_blk->next = NULL;
        new_blk->prev = NULL;
        return new_blk;
    }
    else{
        while(curr != NULL){
            if(new_blk == curr){
                printf("Double free\n");  
                exit(EXIT_FAILURE);
            }
            else if(new_blk < curr){
                if(curr == * free_head){
                    new_blk->prev = curr->prev;
                    new_blk->next = curr;
                    curr->prev = new_blk;
                    * free_head = new_blk;
                    return new_blk;
                }
                else{
                    new_blk->prev = curr->prev;
                    new_blk->next = curr;
                    curr->prev->next = new_blk;
                    curr->prev = new_blk;
                    return new_blk;
                }
            }
            else{
                curr=curr->next;
            }
        }
        if(curr == NULL){
            new_blk->prev = * free_tail;
            (* free_tail)->next = new_blk;
            new_blk->next = NULL;
            * free_tail = new_blk;
            return new_blk;
        }
    }
}
 

void delete_free_block(block_t ** free_head, block_t ** free_tail, block_t * blk){
    if(* free_head == blk && blk->next == NULL){
        * free_head=NULL;
        * free_tail=NULL;
        blk->next = NULL;
        blk->prev = NULL;
    }
    else if(* free_head == blk && blk->next != NULL){
        * free_head=blk->next;
        blk->next->prev = NULL;
        blk->next = NULL;
        blk->prev = NULL;
    }
    else if(blk->prev != NULL && blk->next == NULL){
        * free_tail=blk->prev;
        blk->prev->next = NULL;
        blk->next = NULL;
        blk->prev = NULL;
    }
    else{
        blk->next->prev = blk->prev;
        blk->prev->next = blk->next;
        blk->next = NULL;
        blk->prev = NULL;
    }
}

block_t * split_block(block_t ** free_head, block_t ** free_tail, block_t * ff, size_t size){
    block_t * request_blk = ff->data + size;
    request_blk->data = ff->data + size + sizeof(block_t);
    request_blk->next = ff->next;
    request_blk->prev = ff;
    request_blk->size = ff->size - sizeof(block_t) - size;
    ff->size = size;
    ff->next = request_blk;
    delete_free_block(free_head, free_tail, ff);       
    if(request_blk->next != NULL){
        request_blk->next->prev = request_blk;
    }
    else{
        * free_tail = request_blk;
    }
    return ff;
}

/*
Merge two adjacent free blocks into one.
*/
block_t * merge_block(block_t ** free_tail, block_t * blk){
    blk->size = blk->size + sizeof(block_t) + blk->next->size;
    blk->next = blk->next->next;
    if(blk->next != NULL){
        blk->next->prev = blk;
    }
    else{
        * free_tail  = blk;
    }
    return blk; 
}

/*
Malloc lock.
*/
void * ts_malloc_lock(size_t size){
    block_t * bf_blk = NULL;
    pthread_mutex_lock(&lock);///////////////////////////////////////////////////
    if(size <= 0){
        printf("Malloc failed, size should >0\n");
        pthread_mutex_unlock(&lock);
        exit(EXIT_FAILURE);
    }
    else{        
        bf_blk = bf_find_block(&global_free_head,size);
        if(bf_blk != NULL){    
            if(bf_blk->size < (sizeof(block_t)+size) && bf_blk->size >= size){ 
                delete_free_block(&global_free_head, &global_free_tail, bf_blk);
            }
            else if(bf_blk->size >= (sizeof(block_t)+size)){    
                bf_blk = split_block(&global_free_head, &global_free_tail, bf_blk,size);
            }
        }
        else{
            void * b = sbrk(0);
            if(sbrk(sizeof(block_t)+size) == (void *)(-1)){
                perror("sbrk");
                pthread_mutex_unlock(&lock);
                return NULL;
            }
            // void * b = sbrk(sizeof(block_t)+size);
            bf_blk = b;
            bf_blk->data = b + sizeof(block_t);
            bf_blk->size = size;
            bf_blk->next = NULL;
            bf_blk->prev = NULL;
            data_seg_size = data_seg_size + size;
            if(first_blk == NULL){
                first_blk = bf_blk;
            }      
        }
        pthread_mutex_unlock(&lock);////////////////////////////////////////////////
        return bf_blk->data;
    }
}

/*
Malloc nolock.
*/
void * ts_malloc_nolock(size_t size){
    block_t * bf_blk = NULL;
    if(size <= 0){
        printf("Malloc failed, size should >0\n");
        exit(EXIT_FAILURE);
    }
    else{        
        bf_blk = bf_find_block(&local_free_head,size);
        if(bf_blk != NULL){         
            if(bf_blk->size < (sizeof(block_t)+size) && bf_blk->size >= size){ 
                delete_free_block(&local_free_head, &local_free_tail,bf_blk);
            }
            else if(bf_blk->size >= (sizeof(block_t)+size)){    
                bf_blk = split_block(&local_free_head, &local_free_tail,bf_blk,size);
            }
        }
        else{
            pthread_mutex_lock(&lock);///////////////////////////////////////
            void * b = sbrk(0);
            if(sbrk(sizeof(block_t)+size) == (void *)(-1)){
                perror("sbrk");
                return NULL;
            }
            pthread_mutex_unlock(&lock);//////////////////////////////////////
            bf_blk = b;
            bf_blk->data = b + sizeof(block_t);
            bf_blk->size = size;
            bf_blk->next = NULL;
            bf_blk->prev = NULL;
            data_seg_size = data_seg_size + size;
            if(first_blk==NULL){
                first_blk=bf_blk;
            }      
        }
    }
    return bf_blk->data;
}



/*
Free lock.
*/
void ts_free_lock(void *ptr){
    block_t * free_blk =NULL;
    pthread_mutex_lock(&lock);//////////////////////////////////////////////////////////////////
	if(first_blk != NULL){
		if(ptr > (void *)first_blk && ptr<sbrk(0)){
            free_blk = (void *)(ptr - sizeof(block_t));
            if(free_blk->data != ptr){
                printf("Address error\n");  
                exit(EXIT_FAILURE);
            }
		}
        else{
            printf("Out of heap\n");  
            exit(EXIT_FAILURE);
        }
	}
    else{
        printf("No such to free\n");  
        exit(EXIT_FAILURE);
    }
    block_t * added_free_blk = addto_sorted_free_list(&global_free_head, &global_free_tail, free_blk); 
    if(added_free_blk != free_blk){
        printf("Free memory failed\n");
        exit(EXIT_FAILURE);
    }
    else{
        if(added_free_blk->prev != NULL && added_free_blk->prev->data + added_free_blk->prev->size == added_free_blk){
            added_free_blk = merge_block(&global_free_tail,added_free_blk->prev); 
        }
        if(added_free_blk -> next != NULL && added_free_blk->data + added_free_blk->size == added_free_blk->next){
            added_free_blk = merge_block(&global_free_tail,added_free_blk);

        }        
    }
    pthread_mutex_unlock(&lock);//////////////////////////////////////////////////////////////////
}

/*
Free nolock.
*/
void ts_free_nolock(void *ptr){
    block_t * free_blk =NULL;
	if(first_blk != NULL){
		if(ptr > (void *)first_blk && ptr<sbrk(0)){
            free_blk = (void *)(ptr - sizeof(block_t));
            if(free_blk->data != ptr){
                printf("Address error\n");
                exit(EXIT_FAILURE);
            }
		}
        else{
            printf("Out of heap\n");
            exit(EXIT_FAILURE);
        }
	}
    else{
        printf("No such to free\n"); 
        exit(EXIT_FAILURE);
    }
    block_t * added_free_blk = addto_sorted_free_list(&local_free_head, &local_free_head ,free_blk);
    if(added_free_blk != free_blk){
        printf("Free memory failed\n");
        exit(EXIT_FAILURE);
    }
    else{
        if(added_free_blk->prev != NULL && added_free_blk->prev->data + added_free_blk->prev->size == added_free_blk){ 
            added_free_blk = merge_block(&local_free_tail,added_free_blk->prev); 
        }
        if(added_free_blk -> next != NULL && added_free_blk->data + added_free_blk->size == added_free_blk->next){
            added_free_blk = merge_block(&local_free_tail,added_free_blk);

        }        
    }

}