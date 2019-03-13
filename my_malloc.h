#ifndef __MYMALLOC_H__
#define __MYMALLOC_H__

#include <stdio.h>
#include <stdlib.h>


typedef struct _block_t block_t;
struct _block_t{
    void * data;    
    size_t size;
    block_t * next;
    block_t * prev;
};
unsigned long get_data_segment_size();

unsigned long get_data_segment_free_space_size();

block_t * bf_find_block(block_t ** free_head, size_t size);

block_t * addto_sorted_free_list(block_t ** free_head, block_t ** free_tail, block_t * new_blk);

void delete_free_block(block_t ** free_head, block_t ** free_tail,block_t * blk);

block_t * split_block(block_t ** free_head, block_t ** free_tail,block_t * ff, size_t size);

block_t * merge_block(block_t ** free_tail, block_t * blk);

void * ts_malloc_lock(size_t size);

void * ts_malloc_nolock(size_t size);

void ts_free_lock(void *ptr);

void ts_free_nolock(void *ptr);


#endif