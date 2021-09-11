#ifndef DEDUP_DEGREE_H
#define DEDUP_DEGREE_H

#include <glib.h>
#include <gmodule.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "../duplicates/duplicatedist.h"


typedef struct counter_element {
    uint32_t counter;
    void *element;
} *Counter_Element;

typedef struct counter_by_index_length {
    uint64_t index;
    uint64_t size;
} *Counter_By_Index_Length;

typedef struct sorted_counter_array {
    GArray *sorted_array;
    GHashTable *hash_table;
    GArray *sorted_counter;
    int size_element;
    uint64_t* (*getKey) (void *);
} *Sorted_Counter_Array;

typedef struct dedup_degree {
    struct sorted_counter_array dedup_writes;
    struct sorted_counter_array dedup_reads;
    struct sorted_counter_array offset_writes;
    struct sorted_counter_array offset_reads;
    GArray *unique_blocks_writes;
    GArray *unique_blocks_reads; 
} *Dedup_Degree;


/*******************************  DEDUP_DEGREE *********************************/
Dedup_Degree new_dedup_degree();
void dedup_degree_add_block(Dedup_Degree dd, struct block_info *bi);
void dedup_degree_add_offSet(Dedup_Degree dd, uint64_t offset, bool is_write);

void dedup_degree_add_block_info_write(Dedup_Degree dd, struct block_info* bi);
void dedup_degree_add_block_info_read(Dedup_Degree dd, struct block_info* bi);
void dedup_degree_add_offset_write(Dedup_Degree dd, uint64_t offset);
void dedup_degree_add_offset_read(Dedup_Degree dd, uint64_t offset);
void dedup_degree_print_block_info_write(Dedup_Degree dd);

struct block_info* get_nth_more_duplicate(Dedup_Degree dd, uint32_t nth);
struct block_info* get_nth_most_duplicate(Dedup_Degree dd);
struct block_info* get_nth_less_duplicate(Dedup_Degree dd, uint32_t nth);
struct block_info* get_nth_least_duplicate(Dedup_Degree dd);
struct block_info* get_nth_unique(Dedup_Degree dd, uint32_t nth);

#define block_info_dedup_get_counter(bidc) bidc->counter
#define block_info_dedup_get_block(bidc) bidc->block_info

void dedup_degree_print_unique_array(Dedup_Degree dd, FILE *out);
void dedup_degree_print_blocks(Dedup_Degree dd, FILE *out);
void dedup_degree_print_offset_write(Dedup_Degree dd, FILE *out);
void dedup_degree_print_offset_read(Dedup_Degree dd, FILE *out);
/*******************************************************************************/

#endif