
#include "dedupDegree.h"


/***************************** SORTED COUNTER ARRAY ********************************/

#define get_sorted_counter(sca,i) g_array_index(sca->sorted_counter, Counter_By_Index_Length, i);
#define get_sorted_array(sca,i) &g_array_index(sca->sorted_array, struct counter_element, i);


uint64_t* getKey_block_info(void* bi_ptr){
	struct block_info* bi = (struct block_info*) bi_ptr;
	return &bi->cont_id;
}

uint64_t* getKey_offset(void *offset){
	return (uint64_t *) offset;
}

void sorted_counter_array_add_hash_to_hash_table(Sorted_Counter_Array sca, void * element, uint64_t* i){
	uint64_t *key = sca->getKey(element);
	uint64_t *index = malloc(sizeof(uint64_t));
	*index = *i;
	uint64_t *key_rep = malloc(sizeof(uint64_t));
	*key_rep = *key;
	g_hash_table_insert(sca->hash_table, key_rep, index);
}

void dedup_swap_array(GArray * arr, int *i1, int *i2){
	gconstpointer *p1 = &g_array_index(arr, gconstpointer, *i1);
	gconstpointer *p2 = &g_array_index(arr, gconstpointer, *i2);
	gconstpointer saved_ptr = *p1;
	int saved_i = *i1;

	*i1 = *i2;
	*i2 = saved_i;

	*p1 = *p2;
	*p2 = saved_ptr;
}

Counter_By_Index_Length new_counter_by_index_length(uint64_t index, uint64_t size){
	Counter_By_Index_Length res = malloc(sizeof(struct counter_by_index_length));
	res->index = index;
	res->size = size;
	//printf("new cbil: %lu, %lu\n", res->index, res->size);
	return res;
}

void new_struct_counter_by_index_length(struct counter_by_index_length * cbil, uint64_t index, uint64_t size){
	cbil->index = index;
	cbil->size = size;
	//printf("new cbil: %lu, %lu\n", cbil->index, cbil->size);
}

void new_sorted_counter_array(Sorted_Counter_Array sca, int size_element, uint64_t* (*getkey)(void*)){
	sca->size_element = size_element;
	sca->sorted_array = g_array_new(false, true, sizeof(struct counter_element));
	sca->hash_table = g_hash_table_new_full(g_int64_hash, g_int64_equal, free, free);
	sca->sorted_counter = g_array_new(false, true, sizeof(Counter_By_Index_Length));
	Counter_By_Index_Length single_counters = new_counter_by_index_length(0,0);
	g_array_append_val(sca->sorted_counter, single_counters);
	sca->getKey = getkey;
}

void sorted_counter_array_swap2(Counter_Element ce1, Counter_Element ce2, uint64_t *i1, uint64_t *i2){
	uint64_t saved_i = *i1;
	struct counter_element ce_saved;
	ce_saved.counter = ce1->counter;
	ce_saved.element = ce1->element;
	
	*i1 = *i2;
	*i2 = saved_i;

	ce1->counter = ce2->counter;
	ce1->element = ce2->element;
	ce2->counter = ce_saved.counter;
	ce2->element = ce_saved.element;
}

void sorted_counter_array_swap(GArray * arr, uint64_t *i1, uint64_t *i2){
	gconstpointer *p1 = &g_array_index(arr, gconstpointer, *i1);
	gconstpointer *p2 = &g_array_index(arr, gconstpointer, *i2);
	gconstpointer saved_ptr = *p1;
	uint64_t saved_i = *i1;

	*i1 = *i2;
	*i2 = saved_i;

	*p1 = *p2;
	*p2 = saved_ptr;
}

void sorted_counter_array_incr_element(Sorted_Counter_Array sca, void *element, uint64_t* index){
	Counter_Element ce = get_sorted_array(sca, *index);
	Counter_Element ce2 = NULL;
	ce->counter++;
	//printf("----------i: %lu-----------\n", *index);
	//printf("ce: %d, %lu\n", ce->counter, ((struct block_info*) ce->element)->cont_id);
	bool added = false;
	//printf("ce->counter: %d\n", ce->counter);
	for(int counter = ce->counter-2; counter < sca->sorted_counter->len && !added; counter++){
		Counter_By_Index_Length cbil = get_sorted_counter(sca, counter);
		//printf("trying index: %d\n", counter);
		if(cbil->size > 0){
			//printf("cbil: %lu, %lu, %d\n", cbil->index, cbil->size, sca->sorted_array->len);
			ce2 = get_sorted_array(sca, cbil->index);
			//printf("counter: %d\n", ce2->counter);
			uint64_t *ce2_key = sca->getKey(ce2->element);
			//printf("key: %lu\n", *ce2_key);
			uint64_t *ce2_index = g_hash_table_lookup(sca->hash_table, ce2_key);
			//printf("swapping , i1 %lu, i2 %lu\n", *index, *ce2_index);
			if(!ce2_index){
				sorted_counter_array_add_hash_to_hash_table(sca, ce2->element, &cbil->index);
			} 
			ce2_index = g_hash_table_lookup(sca->hash_table, ce2_key);
			sorted_counter_array_swap2(ce, ce2, index, ce2_index);
			cbil->size--;
			cbil->index++;
			added = true;
			/*else {
				guint size = 0;
				gpointer * arr = g_hash_table_get_keys_as_array(sca->hash_table, &size);
				//printf("c2key: %lu\n", *ce2_key);
				int i = 0;
				for(int i = 0; i < size; i++){
					if(*ce2_key == *((uint64_t *) arr[i])){
						printf("found %lu\n", *ce2_key);
						break;
					}
					//printf("key: %lu\n", *((uint64_t *) arr[i]));
				}
				if(i == size){
					printf("NOT FOUND\n");
				}
			}*/
			//printf("ce: %d, %lu\n", ce->counter, ((struct block_info*) ce->element)->cont_id);
			//printf("ce: %d, %lu\n", ce2->counter, ((struct block_info*) ce2->element)->cont_id);
			//printf("index: %lu\n", *sca->getKey(ce2->element));
			//printf("swaped , i1 %lu, i2 %lu\n", *index, *ce2_index);
			//printf("ce: %d, %lu\n", ce->counter, ((struct block_info*) ce->element)->cont_id);
			//printf("ce: %d, %lu\n", ce2->counter, ((struct block_info*) ce2->element)->cont_id);
			
		}
	}
	//adding the only most counter;
	if(!added){
		Counter_By_Index_Length highest = new_counter_by_index_length(0,1);
		g_array_append_val(sca->sorted_counter, highest);
		if(*index != 0){
			ce2 = get_sorted_array(sca, 0);
			Counter_By_Index_Length cbil = get_sorted_counter(sca, sca->sorted_counter->len-1); 
			uint64_t *ce2_key = sca->getKey(ce2->element);
			uint64_t *ce2_index = g_hash_table_lookup(sca->hash_table, ce2_key);
			if(!ce2_index){
				sorted_counter_array_add_hash_to_hash_table(sca, ce2->element, &cbil->index);
			}
			ce2_index = g_hash_table_lookup(sca->hash_table, ce2_key);
			if(*ce2_index != *index){
				sorted_counter_array_swap2(ce, ce2, index, ce2_index);
			}
			cbil->index++;
			cbil->size--;
			 /*else {
				sorted_counter_array_add_hash_to_hash_table(sca, ce2->element)
			}*/
		} else {
			Counter_By_Index_Length cbil = get_sorted_counter(sca, ce->counter-1);
			cbil->index++;
			cbil->size--;
		}
	} else if(ce2 != NULL) {
		uint32_t counter = ce2->counter;
		if(counter-1 == sca->sorted_counter->len){
			Counter_By_Index_Length highest = new_counter_by_index_length(0,1);
			g_array_append_val(sca->sorted_counter, highest);
		}
		//printf("counter: %d, size: %u", counter, sca->sorted_counter->len);
		Counter_By_Index_Length cbil = get_sorted_counter(sca, counter-1);
		//printf("(%d), (size: %d)\n", counter-2, sca->sorted_counter->len);
		//printf("cbil2: %lu, %lu\n", cbil->index, cbil->size);
		//printf("(I: %lu)ce->counter = %d, len:%d\n", *index, ce2->counter, sca->sorted_counter->len);
		if(cbil->size == 0){
			cbil->index = *index;
			cbil->size = 1;
		} else {
			cbil->size++;
		}
	}
}


void sorted_counter_array_add_element(Sorted_Counter_Array sca, void *element){
	uint64_t *key = sca->getKey(element);
	struct counter_element ce;
	ce.counter = 1;
	ce.element = element;
	g_array_append_val(sca->sorted_array, ce);
	Counter_By_Index_Length cbil = get_sorted_counter(sca, 0);
	cbil->size++;
	uint64_t *index = malloc(sizeof(uint64_t));
	*index = (uint64_t) sca->sorted_array->len-1;
	uint64_t *key_rep = malloc(sizeof(uint64_t));
	*key_rep = *key;
	g_hash_table_insert(sca->hash_table, key_rep, index);
}

void sorted_counter_array_incr(Sorted_Counter_Array sca, void *element){
	uint64_t *key = sca->getKey(element);
	uint64_t *index = g_hash_table_lookup(sca->hash_table, key);
	if(index){
		sorted_counter_array_incr_element(sca, element, index);
	} else {
		sorted_counter_array_add_element(sca, element);
	}
}



/********************************* FAULT DEGREE ************************************/


Dedup_Degree new_dedup_degree(){
	Dedup_Degree res = malloc(sizeof(struct dedup_degree));
	new_sorted_counter_array(&res->dedup_writes, sizeof(struct block_info*), getKey_block_info);
	new_sorted_counter_array(&res->dedup_reads, sizeof(struct block_info*), getKey_block_info);
	new_sorted_counter_array(&res->offset_reads, sizeof(struct block_info*), getKey_offset);
	new_sorted_counter_array(&res->offset_writes, sizeof(struct block_info*), getKey_offset);

	res->unique_blocks_reads = g_array_new(false, false, sizeof(struct block_info*));
	res->unique_blocks_writes = g_array_new(false, false, sizeof(struct block_info*));
	return res;
}



void print_block_info_file(Counter_Element b, FILE *out){
	if(!out) return;
	fprintf(out, "Counter:%u, ", b->counter);
	fprintf(out, "Block Content id:%ld\n", ((struct block_info*)b->element)->cont_id);
}

void print_offset_op(Counter_Element o, FILE *out){
	if(out == NULL) {
		printf("Counter:%u, ", o->counter);
		printf("Offset:%lu\n", *((uint64_t*) o->element));
		return;
	}
	fprintf(out, "Counter:%u, ", o->counter);
	fprintf(out, "Offset:%lu\n", *((uint64_t*) o->element));
}

/*void dedup_degree_print_hash_table(Dedup_Degree dd){
	if(!dd) return;
	GList *keys = g_hash_table_get_keys(dd->contentId_index);
	GList *iter = keys;
    int i = 0;
	while(iter && iter->next){
		uint64_t key_int = (uint64_t) *((uint64_t *)iter->data);
		printf("(%d)Key: %ld -> %d\n", i, key_int, *((int *)g_hash_table_lookup(dd->contentId_index, &key_int)));
		iter = iter->next;
        i++;
	}
	g_list_free(keys);
}*/


void dedup_degree_print_block_info_write(Dedup_Degree dd){
	if(!dd) return;
	printf("dedup_degree_print_block_info_write\n");
	Sorted_Counter_Array sca = &dd->dedup_writes;
	for(int i = 0; i < dd->dedup_writes.sorted_array->len; i++){
		Counter_Element ce = get_sorted_array(sca, i);
		struct block_info *bi = (struct block_info*) ce->element;
		if(ce->counter > 1)
			printf("(%d)- Counter: %d, cont_id:%lu\n", i, ce->counter, bi->cont_id);
	}
}

void dedup_degree_add_offset_write(Dedup_Degree dd, uint64_t offset){
	if(!dd) return;
	uint64_t *offset_ptr = malloc(sizeof(uint64_t));
	*offset_ptr = offset;
	sorted_counter_array_incr(&dd->offset_writes, offset_ptr);
}

void dedup_degree_add_offset_read(Dedup_Degree dd, uint64_t offset){
	if(!dd) return;
	uint64_t *offset_ptr = malloc(sizeof(uint64_t));
	*offset_ptr = offset;
	sorted_counter_array_incr(&dd->offset_reads, offset_ptr);
}

void dedup_degree_add_block_info_write(Dedup_Degree dd, struct block_info* bi){
	if(!dd) return;
	
	if(bi->procid >= 0){
        g_array_append_val(dd->unique_blocks_writes, bi);
    } else {
		sorted_counter_array_incr(&dd->dedup_writes, bi);
	}
	
}

void dedup_degree_add_block_info_read(Dedup_Degree dd, struct block_info* bi){
	if(!dd) return;
	if(bi->procid >= 0){
        g_array_append_val(dd->unique_blocks_reads, bi);
        return;
    }
	sorted_counter_array_incr(&dd->dedup_reads, bi);
}



struct block_info* get_nth_more_duplicate(Dedup_Degree dd, uint32_t nth){
	if(!dd) return NULL;
	Sorted_Counter_Array sca = &dd->dedup_writes;
	if(nth > sca->sorted_array->len){
		nth = sca->sorted_array->len-1;
	}
	Counter_Element counter = get_sorted_array(sca, nth);
	return (struct block_info*) counter->element;
}

struct block_info* get_nth_most_duplicate(Dedup_Degree dd){
	return get_nth_more_duplicate(dd, 0);
}

struct block_info* get_nth_less_duplicate(Dedup_Degree dd, uint32_t nth){
	if(!dd) return NULL;
	Sorted_Counter_Array sca = &dd->dedup_writes;
	if(nth > sca->sorted_array->len){
		nth = 0;
	}
	nth = sca->sorted_array->len-1 - nth;
	Counter_Element counter = get_sorted_array(sca, nth);
	return (struct block_info*) counter->element;
}

struct block_info* get_nth_least_duplicate(Dedup_Degree dd){
	return get_nth_less_duplicate(dd, 0);
}

struct block_info* get_nth_unique(Dedup_Degree dd, uint32_t nth){
	return g_array_index(dd->unique_blocks_writes, struct block_info*, nth);
}

void dedup_degree_print_unique_array(Dedup_Degree dd, FILE *out){
	if(!dd || !out) return;
	fprintf(out, "Unique blocks Writes\nSize: %d\n", dd->unique_blocks_writes->len);
	for(int i = 0; i < dd->unique_blocks_writes->len; i++){
		struct block_info *current_block = g_array_index(dd->unique_blocks_writes, struct block_info*, i);
		fprintf(out, "(%d)-", i);
        fprintf(out, "Block Content id: %ld\n", current_block->cont_id);	
    }
	fprintf(out, "Unique blocks Reads\nSize: %d\n", dd->unique_blocks_reads->len);
	for(int i = 0; i < dd->unique_blocks_reads->len; i++){
		struct block_info *current_block = g_array_index(dd->unique_blocks_reads, struct block_info*, i);
		fprintf(out, "(%d)-", i);
        fprintf(out, "Block Content id: %ld\n", current_block->cont_id);	
    }
}

void dedup_degree_print_blocks(Dedup_Degree dd, FILE *out){
	if(!dd || !out) return;
	Sorted_Counter_Array sca = &dd->dedup_writes;
	GArray *dups = sca->sorted_array;
	fprintf(out, "Duplicated blocks Writes\nSize:%d\n", dups->len);
	for(int i = 0; i < dups->len; i++){
		Counter_Element current_block = get_sorted_array(sca, i);
		fprintf(out, "(%d)-", i);
		print_block_info_file(current_block, out);
	}
	sca = &dd->dedup_reads;
	dups = sca->sorted_array;
	fprintf(out, "Duplicated blocks Reads\nSize:%d\n", dups->len);
	for(int i = 0; i < dups->len; i++){
		Counter_Element current_block = get_sorted_array(sca, i);
		fprintf(out, "(%d)-", i);
		print_block_info_file(current_block, out);
	}
}

void dedup_degree_print_offset_write(Dedup_Degree dd, FILE *out){
	if(!dd || !out) return;
	Sorted_Counter_Array sca = &dd->offset_writes;
	GArray *offsets = sca->sorted_array;
	fprintf(out, "Writes per offset\nSize:%d\n", offsets->len);
	for(int i = 0; i < offsets->len; i++){
		Counter_Element current_offset = get_sorted_array(sca, i);
		//print_offset_op(current_offset, NULL);
		fprintf(out, "(%d)-", i);
		print_offset_op(current_offset, out);
	}
}

void dedup_degree_print_offset_read(Dedup_Degree dd, FILE *out){
	if(!dd || !out) return;
	Sorted_Counter_Array sca = &dd->offset_reads;
	GArray *offsets = sca->sorted_array;
	fprintf(out, "Reads per offset\nSize:%d\n", offsets->len);
	for(int i = 0; i < offsets->len; i++){
		Counter_Element current_offset = get_sorted_array(sca, i);
		//print_offset_op(current_offset, NULL);
		fprintf(out, "(%d)-", i);
		print_offset_op(current_offset, out);
	}
}