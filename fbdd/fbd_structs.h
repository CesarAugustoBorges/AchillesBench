#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <glib.h>
#include <gmodule.h>
#include <openssl/md5.h>
#include <xxhash.h>
//#include <MurmurHash3.h>

#include "fbd_defines.h"

#ifndef FBD_STRUCTS_HEADER
#define FBD_STRUCTS_HEADER

typedef union fbd_hash_gen {
    MD5_CTX md5;
    XXH3_state_t *xxh3_128;
} FBD_Hash_Gen;

typedef union fbd_hash{
    char md5[MD5_DIGEST_LENGTH+1]; //16 + 1
    XXH128_hash_t xxh3_128; // uint64_t * 2
    //uint32_t murmur_x86_128[4]; // 32*4
} FBD_Hash;

typedef struct fbd_user_settings {
    bool block_mode;
    bool hash_mode;
    bool dedup_mode;
    bool device_mode;
} *FBD_User_Settings;


typedef struct fbd_thread_info {
    pthread_cond_t cond;
    pthread_mutex_t lock;
    bool bdus_started;
} *FBD_Thread_Info;

typedef struct fbd_block_fault {
    uint32_t size;
    uint64_t offSet;
} *FBD_Block_Fault;

typedef struct fbd_fault {
    uint8_t operation;
    uint8_t fault;
    bool persistent;
    bool active;
    char *args;
} *FBD_Fault;

typedef FBD_Hash *FBD_Hash_Fault;

typedef FBD_Hash_Fault FBD_Dedup_Fault;

/*typedef union fbd_fault_interseption {
    FBD_Hash_Fault hash;
    FBD_Dedup_Fault dedup;
    FBD_Block_Fault block;
} FBD_Fault_Interseption;*/

typedef struct fbd_range_fault {
    uint8_t mode;
    uint8_t size;
    FBD_Fault faults[MAX_FAULTS];
    void *ptr;
} *FBD_Range_Fault;


typedef struct fbd_device {
    char *path;
    int index;
    int fd;
    uint64_t size;
    uint32_t logical_block_size;
    FBD_Thread_Info thread_info;
    FBD_User_Settings user_settings;
    GSList *blocks_with_fault;
    uint8_t hash_type;
    FBD_Hash_Gen hash_gen;
} *FBD_Device;


void fbd_print_hash(uint8_t type, FBD_Hash *hash);
FBD_Device fbd_new_device(int fd);
void fbd_print_user_settings(FBD_Device device);

int fbd_check_and_inject_fault(FBD_Device dev, char* buffer, uint32_t size,
                                                uint64_t offset, uint8_t operation);
int fbd_check_and_inject_write_fault(FBD_Device dev, char* buffer, uint32_t size, uint64_t offset);
int fbd_check_and_inject_read_fault(FBD_Device dev, char* buffer, uint32_t size, uint64_t offset);

/***************************************** Bit Flip ********************************************/
// Generalization
int fbd_add_bit_flip_block_fault(FBD_Device device, uint32_t size, uint64_t offSet, 
                                                uint8_t operation, bool persistent);
int fbd_add_bit_flip_hash_fault(FBD_Device device, union fbd_hash *hash, uint8_t operation, bool persistent);
int fbd_add_bit_flip_dedup_fault(FBD_Device device, union fbd_hash *hash, uint8_t operation, bool persistent);

// Block Mode
int fbd_add_bit_flip_block_write_fault(FBD_Device device, uint32_t size, uint64_t offSet, bool persistent);
int fbd_add_bit_flip_block_read_fault(FBD_Device device, uint32_t size, uint64_t offSet, bool persistent);

// Hash Mode
int fbd_add_bit_flip_hash_write_fault(FBD_Device device, union fbd_hash *hash, bool persistent);
int fbd_add_bit_flip_hash_read_fault(FBD_Device device, union fbd_hash *hash, bool persistent);

// Dedup Mode 
int fbd_add_bit_flip_dedup_write_fault(FBD_Device device, union fbd_hash *hash, bool persistent);
int fbd_add_bit_flip_dedup_read_fault(FBD_Device device, union fbd_hash *hash, bool persistent);

/**************************************** Slow Disk *********************************************/
// Generalization
int fbd_add_slow_disk_block_fault(FBD_Device device, uint32_t size, 
                            uint64_t offSet, uint8_t operation, bool persistent, uint64_t ms);
int fbd_add_slow_disk_hash_fault(FBD_Device device, union fbd_hash *hash, uint8_t operation, 
                                                                bool persistent, uint64_t ms);
int fbd_add_slow_disk_dedup_fault(FBD_Device device, union fbd_hash *hash, uint8_t operation, 
                                                                bool persistent, uint64_t ms);

// Block Mode
int fbd_add_slow_disk_block_write_fault(FBD_Device device, uint32_t size, uint64_t offSet, 
                                                                bool persistent, uint64_t ms);
int fbd_add_slow_disk_block_read_fault(FBD_Device device, uint32_t size, uint64_t offSet, 
                                                                bool persistent, uint64_t ms);

// Hash Mode
int fbd_add_slow_disk_hash_write_fault(FBD_Device device, union fbd_hash *hash, 
                                                            bool persistent, uint64_t ms);
int fbd_add_slow_disk_hash_read_fault(FBD_Device device, union fbd_hash *hash, 
                                                            bool persistent, uint64_t ms);

// Dedup Mode
int fbd_add_slow_disk_dedup_write_fault(FBD_Device device, union fbd_hash *hash, bool persistent, 
                                                                            uint64_t ms);
int fbd_add_slow_disk_dedup_read_fault(FBD_Device device, union fbd_hash *hash, bool persistent, 
                                                                            uint64_t ms);

/************************************** Medium Error *******************************************/
// Generalization
int fbd_add_medium_error_block_fault(FBD_Device device, uint32_t size, uint64_t offSet,
                                                         uint8_t op, bool persistent);
int fbd_add_medium_error_hash_fault(FBD_Device device, union fbd_hash *hash, uint8_t op, bool persistent);
int fbd_add_medium_error_dedup_fault(FBD_Device device, union fbd_hash *hash, uint8_t op, bool persistent);

// Block Mode
int fbd_add_medium_error_block_write_fault(FBD_Device device, uint32_t size, 
                                            uint64_t offSet, bool persistent);
int fbd_add_medium_error_block_read_fault(FBD_Device device, uint32_t size, 
                                            uint64_t offSet, bool persistent);

// Hash Mode
int fbd_add_medium_error_hash_write_fault(FBD_Device device, union fbd_hash *hash, bool persistent);
int fbd_add_medium_error_hash_read_fault(FBD_Device device, union fbd_hash *hash, bool persistent);

// Dedup Mode
int fbd_add_medium_error_dedup_read_fault(FBD_Device device, union fbd_hash *hash, bool persistent);
int fbd_add_medium_error_dedup_read_fault(FBD_Device device, union fbd_hash *hash, bool persistent);

void printBuffer(char *buffer, int start, int size);

/****************************************** Removers ****************************************/
int fbd_remove_all_faults(FBD_Device dev);

#endif