//#define _POSIX_C_SOURCE 200809L
//#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

#include "fbd_structs.h"
#include "./fault/fault.h"

#define BLOCK_SIZE 4096

//************************************** Utils ***********************************************

bool intersept_memory(uint64_t f1, uint64_t f2, uint64_t m1, uint64_t m2, uint64_t *x1, uint64_t *x2){
    *x1 = MAX(f1,m1);
    *x2 = MIN(f2, m2);
    if(*x1 > *x2) return false;
    return true;
}

//It will check max size + max offSet of device in further development
int check_size_offSet(int size, int offSet){
    return FBD_STS_OK;
}

void fbd_string_to_hash(FBD_Device dev, char *string, uint32_t string_size, FBD_Hash *out){
    if(dev->hash_type == FBD_HASH_MD5){
        out->md5[MD5_DIGEST_LENGTH] = '\0';
        MD5_CTX *ctx = &dev->hash_gen.md5;
        MD5_Init(ctx);
        MD5_Update(ctx, (const unsigned char*) string, (size_t) string_size);
        MD5_Final((unsigned char*) out->md5, ctx);
    } else if(dev->hash_type == FBD_HASH_XXH3_128){
        XXH3_state_t *state = dev->hash_gen.xxh3_128;
        if (XXH3_128bits_reset(state) == XXH_ERROR) abort();
        if (XXH3_128bits_update(state, string, string_size) == XXH_ERROR) abort();
        out->xxh3_128 = XXH3_128bits_digest(state);
        //printf("Generated hash: %lu, %lu\n", out->xxh3_128.low64, out->xxh3_128.high64);
    } /*else if(dev->hash_type == FBD_HASH_MURMUR_x86_128){
        //MurmurHash3_x86_128(string, string_size, 0, out->murmur_x86_128);
    }*/ else {
        printf("ERROR identifying the hash type, aborting...\n");
        abort();
    }
}

int fbd_hash_compare(FBD_Device dev, FBD_Hash *h1, FBD_Hash *h2){
    if(dev->hash_type == FBD_HASH_MD5){
        return strcmp(h1->md5, h2->md5) == 0;
    } else if(dev->hash_type == FBD_HASH_XXH3_128){
//	printf("l: %lu, h%lu,\nl: %lu, h:%lu\n", h1->xxh3_128.low64, h2->xxh3_128.low64, h1->xxh3_128.high64, h2->xxh3_128.high64 );
        return h1->xxh3_128.low64 == h2->xxh3_128.low64 && 
               h1->xxh3_128.high64 == h2->xxh3_128.high64;
    } /*else if(dev->hash_type == FBD_HASH_MURMUR_x86_128) {
        for(int i = 0; i < 4; i++){
            if(h1->murmur_x86_128[i] != h2->murmur_x86_128[i])
                return 0;
        }
        return 1;
    }*/ else {
        printf("ERROR identifying the hash type, aborting...\n");
        abort();
    }
    return -1;
}


//********************************** Contructors ********************************************

FBD_Thread_Info fbd_new_thread_info(){
    FBD_Thread_Info thread_info = malloc(sizeof(struct fbd_thread_info));
    thread_info->cond = (pthread_cond_t) PTHREAD_COND_INITIALIZER;
    thread_info->lock = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
    thread_info->bdus_started = false;
    return thread_info;
}

FBD_User_Settings fbd_new_user_settings(){
    FBD_User_Settings user_settings = malloc(sizeof(struct fbd_user_settings));
    user_settings->block_mode = false;
    user_settings->hash_mode = false;
    user_settings->dedup_mode = false;
    return user_settings;
}

FBD_Device fbd_new_device(int fd){
    FBD_Device device = malloc(sizeof(struct fbd_device));
    device->index = -1;
    device->path = NULL;
    device->fd = fd;
    device->thread_info = fbd_new_thread_info();
    device->user_settings = fbd_new_user_settings();
    return device;
}

FBD_Block_Fault fbd_new_block_fault(uint32_t size, uint64_t offSet){
    FBD_Block_Fault block = (FBD_Block_Fault) malloc(sizeof(struct fbd_block_fault));
    block->size = size;
    block->offSet = offSet;
    return block;
}

FBD_Hash_Fault fbd_new_hash_fault(union fbd_hash *hash){
    int size = sizeof(union fbd_hash);
    FBD_Hash_Fault hash_fault = (FBD_Hash_Fault) malloc(size);
    memcpy(hash_fault, hash, size);
    return hash_fault;
}

FBD_Range_Fault fbd_new_range_fault(uint8_t mode){
    FBD_Range_Fault block_fault = malloc(sizeof(struct fbd_range_fault));
    block_fault->size = 0;
    block_fault->mode = mode;
    block_fault->ptr = NULL;
    return block_fault;
}

FBD_Dedup_Fault fbd_new_dedup_fault(union fbd_hash *hash){
    return fbd_new_hash_fault(hash);
}

FBD_Fault fbd_new_fault(uint8_t operation, uint8_t fault_type, bool persistent, void* args, uint8_t args_size){
    FBD_Fault fault = malloc(sizeof(struct fbd_fault));
    fault->operation = operation;
    fault->persistent = persistent;
    fault->fault = fault_type;
    fault->active = true;
    if(args){
        fault->args = malloc(sizeof(args_size));
        memcpy(fault->args, args, args_size);
    }
    return fault;
}

// ***************************************** PRINTS **********************************************

void fbd_print_hash(uint8_t type, FBD_Hash *hash){
    switch(type){
        case FBD_HASH_MD5:
            printf("MD5: %s\n", hash->md5);
        case FBD_HASH_XXH3_128:
            printf("XX3_128: %lu, %lu\n", hash->xxh3_128.low64, hash->xxh3_128.high64);
        break;
    }
}

void fbd_print_user_settings(FBD_Device device){
    printf("*********************** User settings ***********************\n");
    printf("Using block mode:  %s\n", device->user_settings->block_mode ? "Yes" : "No");
    printf("Using hash  mode:  %s\n", device->user_settings->hash_mode  ? "Yes" : "No");
    if(device->user_settings->hash_mode){
        switch(device->hash_type){
            case FBD_HASH_MD5:
                printf("Using MD5 for hashes\n");
            case FBD_HASH_XXH3_128:
                printf("Using XXH3_128 for hashes\n");
            default:
                printf("UNKNOWN HASH CONFIGURATION\n");
        }
    }
    printf("Using dedup mode:  %s\n", device->user_settings->dedup_mode ? "Yes" : "No");
    printf("Using device mode: %s\n", device->user_settings->device_mode? "Yes" : "No");
    printf("*************************************************************\n");
}

void fbd_print_block_fault(FBD_Block_Fault bf){
    printf("size: %d; offSet: %lu\n", bf->size, bf->offSet);
}

void fbd_print_hash_fault(FBD_Hash_Fault hf, uint8_t hash_type){
    if(hash_type == FBD_HASH_XXH3_128){
        printf("hash (XXH3_128): %lu, %lu\n", hf->xxh3_128.low64, hf->xxh3_128.high64);
    } else if(hash_type == FBD_HASH_MD5){
        printf("hash (MD5): %s\n", hf->md5);
    } /*else if(hash_type == FBD_HASH_MURMUR_x86_128){
        printf("hash (Murmur_x86_128): %d, %d, %d, %d\n", hf->murmur_x86_128[0],
            hf->murmur_x86_128[1], hf->murmur_x86_128[2], hf->murmur_x86_128[3]);
    }*/
}

void fbd_print_dedup_fault(FBD_Dedup_Fault df, uint8_t hash_type){
    fbd_print_hash_fault((FBD_Hash_Fault) df, hash_type);
}

void fbd_print_fault(FBD_Fault f){
    printf("************************* Fault *****************************\n");
    printf("Fault:%s , operation: %s, persistent? %s\n",fbd_fault_to_string(f->fault), 
    fbd_operation_to_string(f->operation), f->persistent ? "Yes" : "No");
    printf("active? %s", f->active ? "Yes" : "No");
    if(f->fault == FBD_FAULT_SLOW_DISK){
        printf(", Delay: %lu", *((uint64_t*) f->args));
    }
    printf("\n");
}

void fbd_print_range_fault(FBD_Range_Fault range, uint8_t hash_type){
    printf("*********************** Range Fault *************************\n");
    printf("mode: %s\n", fbd_mode_to_string(range->mode));
    for(int i = 0; i < range->size; i++){
        FBD_Fault fault = range->faults[i];
        printf("fault: %s, op: %s\n", fbd_fault_to_string(fault->fault), 
                                    fbd_operation_to_string(fault->operation));
    }
    if(range->mode == FBD_MODE_BLOCK && range->ptr){
        FBD_Block_Fault bf = (FBD_Block_Fault) range->ptr;
        fbd_print_block_fault(bf);
    } else if(range->mode == FBD_MODE_HASH && range->ptr){
        FBD_Hash_Fault hf = (FBD_Hash_Fault) range->ptr;
        fbd_print_hash_fault(hf, hash_type);
    } else if(range->mode == FBD_MODE_DEDUP && range->ptr) {
        FBD_Dedup_Fault df = (FBD_Dedup_Fault) range->ptr;
        fbd_print_dedup_fault(df, hash_type);
    } else {
        printf("MODE NOT FOUND\n");
    }
    printf("*************************************************************\n");
}

// ***************************************** GETTERS *********************************************

FBD_Range_Fault get_range_block(FBD_Device device, uint32_t size, uint64_t offSet){
    GSList *ranges = device->blocks_with_fault;
    while(ranges != NULL){
        FBD_Range_Fault range = (FBD_Range_Fault) ranges->data;
        if(range->mode == FBD_MODE_BLOCK){
            FBD_Block_Fault block = (FBD_Block_Fault) range->ptr;
            if(block->size == size && block->offSet == offSet){
                return range;
            }
        }
        ranges = g_slist_next(ranges);
    }
    return NULL;
}

FBD_Range_Fault get_range_hash(FBD_Device device, union fbd_hash *hash){
    GSList *ranges = device->blocks_with_fault;
    while(ranges != NULL){
        FBD_Range_Fault range = ranges->data;
        if(range->mode == FBD_MODE_HASH && 
                fbd_hash_compare(device, hash, (FBD_Hash_Fault) range->ptr)){
            return range;
        }
        ranges = g_slist_next(ranges);
    }
    return NULL;
}

FBD_Range_Fault get_range_dedup(FBD_Device device, union fbd_hash* hash){
    GSList *ranges = device->blocks_with_fault;
    while(ranges != NULL){
         FBD_Range_Fault range = ranges->data;
        if(range->mode == FBD_MODE_DEDUP && 
                fbd_hash_compare(device, hash, (FBD_Hash_Fault) range->ptr)){
            return range;
        }
        ranges = g_slist_next(ranges);
    }
    return NULL;
}

FBD_Fault get_fault(FBD_Range_Fault block, uint8_t fault_type, uint8_t op){
    for(int i = 0; i < block->size; i++){
        FBD_Fault fault = block->faults[i];
        if(fault->fault == fault_type && fault->operation & op)
            return fault;
    }
    return NULL;
}

FBD_Fault get_fault_from_mode(FBD_Range_Fault range, uint fault_type, uint8_t op, uint8_t mode){
    if(range->mode == mode){
        return get_fault(range, fault_type, op);
    }
    return NULL;
}


// ******************************************ADDERS **********************************************


void add_fault(FBD_Range_Fault range_fault, FBD_Fault fault){
    fbd_print_range_fault(range_fault, FBD_HASH_XXH3_128);
    range_fault->faults[range_fault->size] = fault;
    range_fault->size++;
}

void add_range_fault(FBD_Device device, FBD_Range_Fault range_fault){
    device->blocks_with_fault = g_slist_prepend(device->blocks_with_fault, (gpointer) range_fault);
}

int fbd_add_block_fault_with_operation(FBD_Device device, uint32_t size, uint64_t offSet, 
                                                        uint32_t fault, bool persistent,
                                                        uint32_t op, void* extra, 
                                                        uint8_t extra_size){
    FBD_Range_Fault bf = get_range_block(device, size, offSet);
    FBD_Fault f = NULL;
    if(!bf){
        bf = fbd_new_range_fault(FBD_MODE_BLOCK);
        bf->ptr = (void *) fbd_new_block_fault(size, offSet);
        f = fbd_new_fault(op, fault, persistent, extra, extra_size);
        add_fault(bf, f);
        add_range_fault(device, bf);
    } else {
        f = get_fault(bf, fault, op);
        if(!f){
            f = fbd_new_fault(op, fault, persistent, extra, extra_size);
            add_fault(bf, f);
        }
        else{
            return FBD_STS_DUP_FAULT;
        }    
    }    
    return FBD_STS_OK;
}

int fbd_add_hash_fault_with_operation(FBD_Device device, union fbd_hash *hash, uint32_t fault, 
                                                        bool persistent, uint32_t op, 
                                                        void* extra, uint8_t extra_size){
    FBD_Range_Fault bf = get_range_hash(device, hash);
    FBD_Fault f = NULL;
    if(!bf){
        bf = fbd_new_range_fault(FBD_MODE_HASH);
        bf->ptr = (void *) fbd_new_hash_fault(hash);
        f = fbd_new_fault(op, fault, persistent, extra, extra_size);
        add_fault(bf, f);
        add_range_fault(device, bf);
    } else {
        f = get_fault(bf, fault, op);
        if(!f){
            f = fbd_new_fault(op, fault, persistent, extra, extra_size);
            add_fault(bf, f);
        }
        else{
            return FBD_STS_DUP_FAULT;
        }    
    }    
    return FBD_STS_OK;
}

int fbd_add_dedup_fault_with_operation(FBD_Device device, union fbd_hash* hash, uint32_t fault,
                                                        bool persistent, uint32_t op, 
                                                        void* extra, uint8_t extra_size){
    FBD_Range_Fault bf = get_range_dedup(device, hash);
    FBD_Fault f = NULL;
    if(!bf){
        bf = fbd_new_range_fault(FBD_MODE_DEDUP);
        bf->ptr = (void *) fbd_new_dedup_fault(hash);
        f = fbd_new_fault(op, fault, persistent, extra, extra_size);
        add_fault(bf, f);
        add_range_fault(device, bf);
    } else {
        f = get_fault(bf, fault, op);
        if(!f){
            f = fbd_new_fault(op, fault, persistent, extra, extra_size);
            add_fault(bf, f);
        }
        else{
            return FBD_STS_DUP_FAULT;
        }    
    }    
    return FBD_STS_OK;
}

int fbd_add_bit_flip_block_fault(FBD_Device device, uint32_t size, uint64_t offSet, 
                                                    uint8_t operation, bool persistent){
    int res_status = check_size_offSet(size, offSet);
    if(res_status != FBD_STS_OK) return res_status;

    res_status = fbd_add_block_fault_with_operation(device, size, offSet, 
                            FBD_FAULT_BIT_FLIP, persistent, operation, NULL, 0);
    return res_status;
}

int fbd_add_bit_flip_hash_fault(FBD_Device device, union fbd_hash *hash, uint8_t operation, bool persistent){
    return fbd_add_hash_fault_with_operation(device, hash, FBD_FAULT_BIT_FLIP, persistent,
                                                                    operation, NULL, 0);
}

int fbd_add_bit_flip_dedup_fault(FBD_Device device, union fbd_hash *hash, uint8_t operation, bool persistent){
    return fbd_add_dedup_fault_with_operation(device, hash, FBD_FAULT_BIT_FLIP, persistent,
                                                                    operation, NULL, 0);
}

int fbd_add_bit_flip_block_write_fault(FBD_Device device, uint32_t size, uint64_t offSet, 
                                                                        bool persistent){
    return fbd_add_bit_flip_block_fault(device, size, offSet, FBD_OP_WRITE, persistent);
}

int fbd_add_bit_flip_block_read_fault(FBD_Device device, uint32_t size, uint64_t offSet,
                                                                        bool persistent){
    return fbd_add_bit_flip_block_fault(device, size, offSet, FBD_OP_READ, persistent);
}

int fbd_add_bit_flip_hash_write_fault(FBD_Device device, union fbd_hash *hash, bool persistent){
    return fbd_add_bit_flip_hash_fault(device, hash, FBD_OP_WRITE, persistent);
}

int fbd_add_bit_flip_hash_read_fault(FBD_Device device, union fbd_hash *hash, bool persistent){
    return fbd_add_bit_flip_hash_fault(device, hash, FBD_OP_READ, persistent);
}

int fbd_add_bit_flip_dedup_write_fault(FBD_Device device, union fbd_hash *hash, bool persistent){
    return fbd_add_bit_flip_dedup_fault(device, hash, FBD_OP_WRITE, persistent);
}

int fbd_add_bit_flip_dedup_read_fault(FBD_Device device, union fbd_hash *hash, bool persistent){
    return fbd_add_bit_flip_dedup_fault(device, hash, FBD_OP_READ, persistent);
}


int fbd_add_slow_disk_block_fault(FBD_Device device, uint32_t size, 
                            uint64_t offSet, uint8_t operation, bool persistent, uint64_t ms){
    int res_status = check_size_offSet(size, offSet);
    if(res_status != FBD_STS_OK) return res_status;
    if(ms < 0) return FBD_STS_WRONG_INPUT;

    res_status = fbd_add_block_fault_with_operation(device, size, offSet, 
                            FBD_FAULT_SLOW_DISK, persistent, operation, &ms, sizeof(uint64_t));
    return res_status;
}

int fbd_add_slow_disk_hash_fault(FBD_Device device, union fbd_hash *hash, uint8_t operation,
                                                    bool persistent, uint64_t ms){
    return fbd_add_hash_fault_with_operation(device, hash, FBD_FAULT_SLOW_DISK, persistent, 
                                                            operation, &ms, sizeof(uint64_t));
}

int fbd_add_slow_disk_dedup_fault(FBD_Device device, union fbd_hash *hash, uint8_t operation,
                                                    bool persistent, uint64_t ms){
    return fbd_add_dedup_fault_with_operation(device, hash, FBD_FAULT_SLOW_DISK, persistent,
                                                            operation, &ms, sizeof(uint64_t));
}

int fbd_add_slow_disk_block_write_fault(FBD_Device device, uint32_t size, uint64_t offSet, 
                                                                bool persistent, uint64_t ms){
    return fbd_add_slow_disk_block_fault(device, size, offSet, FBD_OP_WRITE, persistent, ms);
}

int fbd_add_slow_disk_block_read_fault(FBD_Device device, uint32_t size, uint64_t offSet, 
                                                                bool persistent, uint64_t ms){
    return fbd_add_slow_disk_block_fault(device, size, offSet, FBD_OP_READ, persistent, ms);
}

int fbd_add_slow_disk_hash_write_fault(FBD_Device device, union fbd_hash *hash, bool persistent, 
                                                                            uint64_t ms){
    return fbd_add_slow_disk_hash_fault(device, hash, FBD_OP_WRITE, persistent, ms);
}
int fbd_add_slow_disk_hash_read_fault(FBD_Device device, union fbd_hash *hash, bool persistent, 
                                                                            uint64_t ms){
    return fbd_add_slow_disk_hash_fault(device, hash, FBD_OP_READ, persistent, ms);
}

int fbd_add_slow_disk_dedup_write_fault(FBD_Device device, union fbd_hash *hash, bool persistent,
                                                                             uint64_t ms){
    return fbd_add_slow_disk_dedup_fault(device, hash, FBD_OP_WRITE, persistent, ms);
}
int fbd_add_slow_disk_dedup_read_fault(FBD_Device device, union fbd_hash *hash, bool persistent,
                                                                            uint64_t ms){
    return fbd_add_slow_disk_dedup_fault(device, hash, FBD_OP_READ, persistent, ms);
}


int fbd_add_medium_error_block_fault(FBD_Device device, uint32_t size, uint64_t offSet,
                                                         uint8_t operation, bool persistent){
    int res_status = check_size_offSet(size, offSet);
    if(res_status != FBD_STS_OK) return res_status;

    res_status = fbd_add_block_fault_with_operation(device, size, offSet,
                            FBD_FAULT_MEDIUM, persistent, operation, NULL, 0);

    return res_status;
}

int fbd_add_medium_error_hash_fault(FBD_Device device, union fbd_hash *hash, uint8_t op, bool persistent){
    return fbd_add_hash_fault_with_operation(device, hash, FBD_FAULT_MEDIUM, persistent, op, NULL, 0);
}

int fbd_add_medium_error_dedup_fault(FBD_Device device, union fbd_hash *hash, uint8_t op, bool persistent){
    return fbd_add_dedup_fault_with_operation(device, hash, FBD_FAULT_MEDIUM, persistent, op, NULL, 0);
}

int fbd_add_medium_error_block_write_fault(FBD_Device device, uint32_t size, uint64_t offSet, bool persistent){
    return fbd_add_medium_error_block_fault(device, size, offSet, FBD_OP_WRITE, persistent);
}

int fbd_add_medium_error_block_read_fault(FBD_Device device, uint32_t size, uint64_t offSet, bool persistent){
    return fbd_add_medium_error_block_fault(device, size, offSet, FBD_OP_READ, persistent);
}

int fbd_add_medium_error_hash_write_fault(FBD_Device device, union fbd_hash *hash, bool persistent){
    return fbd_add_medium_error_hash_fault(device, hash, FBD_OP_WRITE, persistent);
}

int fbd_add_medium_error_hash_read_fault(FBD_Device device, union fbd_hash *hash, bool persistent){
    return fbd_add_medium_error_hash_fault(device, hash, FBD_OP_READ, persistent);
}

int fbd_add_medium_error_dedup_write_fault(FBD_Device device, union fbd_hash *hash, bool persistent){
    return fbd_add_medium_error_dedup_fault(device, hash, FBD_OP_WRITE, persistent);
}

int fbd_add_medium_error_dedup_read_fault(FBD_Device device, union fbd_hash *hash, bool persistent){
    return fbd_add_medium_error_dedup_fault(device, hash, FBD_OP_READ, persistent);
}

int fbd_check_dedup_injection_exceptions(FBD_Device dev, char* buffer, uint32_t size, 
                                    uint64_t offset, uint8_t operation, FBD_Range_Fault range){
    FBD_Fault f = NULL;
    //printf("Checking excepions for dedup\n");
    //exceptional case for bit flip dedup on write
    if((f = get_fault_from_mode(range, FBD_FAULT_BIT_FLIP, FBD_OP_WRITE, FBD_MODE_DEDUP))){
        if(f->active && operation == FBD_OP_READ){
            fl_inject_bit_flip_fault_buffer(buffer, size);
            pwrite(dev->fd, buffer, size, offset);
            //printf("------- Injected dedup bf fault, offset: %lu, size: %d -------\n", offset, size);
            //printf("(%d) buf: %s\n", w, buffer);
            f->active = false;
        }/* else if(!f->active && operation == FBD_OP_READ){
            //printf("------- Fault already injected , offset: %lu, size: %d -------\n", offset, size);
        }*/
    }
    //printf("%p\n", f);
    return FBD_STS_OK;
}

/************************************** REMOVERS ********************************************/

void fbd_free_range_fault(FBD_Range_Fault range){
    free(range->ptr);
    free(range);
}

void fbd_free_fault(gpointer p){
    fbd_free_range_fault((FBD_Range_Fault) p);
}

int fbd_remove_all_faults(FBD_Device dev){
    g_slist_free_full(dev->blocks_with_fault, fbd_free_fault);
    dev->blocks_with_fault = NULL;
    printf("Removed all faults\n");
    return FBD_STS_OK;
}


/************************************** INJECTORS *******************************************/

int fbd_check_and_inject_fault(FBD_Device dev, char* buffer, uint32_t size, 
                                            uint64_t offset, uint8_t operation){
    uint64_t inter_mem[2];
    int status = FBD_STS_OK;
    bool intersepted = false;
    bool found_fault = false;
    FBD_Hash hash;
    if(dev->user_settings->hash_mode || dev->user_settings->dedup_mode){
        fbd_string_to_hash(dev, buffer, 4096, &hash);
    }
    GSList *ranges = dev->blocks_with_fault;
    FBD_Range_Fault range_to_remove = NULL; 
    int n = -1;
    for(; ranges && status == FBD_STS_OK && !found_fault; ranges = g_slist_next(ranges)){
        n++;
        FBD_Range_Fault range = (FBD_Range_Fault) ranges->data;
        if(range->mode == FBD_MODE_BLOCK){
            FBD_Block_Fault bf = (FBD_Block_Fault) range->ptr;
            intersepted = intersept_memory(bf->offSet, bf->offSet + bf->size -1,
                                                    offset, offset + size -1,
                                                    inter_mem, inter_mem+1);
        } else if(range->mode == FBD_MODE_HASH || range->mode == FBD_MODE_DEDUP){
            FBD_Hash *hf = (FBD_Hash*) range->ptr;
            intersepted = fbd_hash_compare(dev, &hash, hf);
	    //printf("intersepted :%d\n", intersepted);
        } else {
            status = FBD_STS_WRONG_MODE;
        }
        if(status == FBD_STS_OK && intersepted){
            //fbd_print_range_fault(range);
            fbd_check_dedup_injection_exceptions(dev, buffer, size, offset, operation, range);
            int j = 0;
            for(; j < range->size; j++){
                FBD_Fault cur_fault = range->faults[j];
                //printf("(%d) f:%d, op:%d\n", j, range->faults[j]->fault, range->faults[j]->operation);
                if(cur_fault->operation & operation && cur_fault->active){
                    switch (cur_fault->fault){
                    case FBD_FAULT_BIT_FLIP:
                        fl_inject_bit_flip_fault_buffer(buffer, size);
                        break;
                    case FBD_FAULT_SLOW_DISK:;
                        uint64_t ms = *((uint64_t *)cur_fault->args);
                        fl_inject_slow_disk_fault(ms);
                        break;
                    case FBD_FAULT_MEDIUM:;
                        status = fl_inject_medium_disk_fault();
                        break;
                    default:
                        break;
                    }
                    if((status || status == FBD_STS_MEDIUM_ERROR) && !cur_fault->persistent){
                        //printf("SHIFTING...\n");
                        for(int k = j+1; k < range->size; k++){
                            range->faults[k-1] = range->faults[k];
                        }
                        range->size--;
                        if(range->size == 0){
                            range_to_remove = range;
                        }
                    }
                }
            }
            found_fault = true;
        }
    }

    if(range_to_remove){
        dev->blocks_with_fault = g_slist_remove(dev->blocks_with_fault, range_to_remove);
    }
    return status;
}

int fbd_check_and_inject_write_fault(FBD_Device dev, char* buffer, uint32_t size, uint64_t offset){
    return fbd_check_and_inject_fault(dev, buffer, size, offset, FBD_OP_WRITE);
}
int fbd_check_and_inject_read_fault(FBD_Device dev, char* buffer, uint32_t size, uint64_t offset){
    return fbd_check_and_inject_fault(dev, buffer, size, offset, FBD_OP_READ);
}

void printBuffer(char *buffer, int start, int size){
    printf("Buffer:\"");
    for(int i = start; i < start + size; i++){
        printf("%c", buffer[i]);
    }
    printf("\"\n");
}
