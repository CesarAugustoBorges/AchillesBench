#include <stdint.h>

#include <fsp_structs.h>

#ifndef FSP_CLIENT_HEADER
#define FSP_CLIENT_HEADER

int fsp_socket();
void fsp_connect();

/************************************* CACHE ********************************************/

typedef struct fsp_cache{
    GHashTable *res_block;
    GHashTable *res_hash;
    GHashTable *res_dedup;
    GArray *res_full_disk;
} *FSP_Cache;

typedef struct fsp_fault_res{
    uint8_t fault;
    int res;
} *FSP_Fault_Res;

void fsp_cache_init();
FSP_Cache fsp_new_cache();
void fsp_cache_update_response(uint8_t mode, void* key, uint8_t fault, FSP_Response res);
FSP_Response fsp_cache_get_fault_response(uint8_t mode, void* key, uint8_t fault);
bool fsp_cache_has_fault_by_mode(uint8_t mode, void* key, uint8_t fault);
bool fsp_cache_has_fault(uint64_t *offSet, char* hash, uint8_t fault);
GArray* fsp_cache_get_responses(uint8_t mode, void* key);


/******************************** FAULTS REQUESTS ***************************************/
//Generic
FSP_Response fsp_add_generic_block(int socket, uint32_t size, uint64_t offSet, uint8_t operation, 
                                        int fault, bool persistant, void* extra, int extra_size);
FSP_Response fsp_add_generic_hash(int socket, uint32_t size, char* content, uint8_t operation,
                                        int fault, bool persistant, uint8_t hash_type,
                                        void* extra, int extra_size);
FSP_Response fsp_add_generic_dedup(int socket, uint32_t size, char* content, uint8_t operation, 
                                        int fault, bool persistant, uint8_t hash_type,
                                        void* extra, int extra_size);


/*********************************** Bit Flip *******************************************/

//Block Mode
FSP_Response fsp_add_bit_flip_block(int socket, uint32_t size, uint64_t offSet,
                                        uint8_t operation, bool persistent);
FSP_Response fsp_add_bit_flip_block_write(int socket, uint32_t size, uint64_t offSet,
                                        bool persistent);
FSP_Response fsp_add_bit_flip_block_read(int socket, uint32_t size, uint64_t offSet,
                                        bool persistent);
FSP_Response fsp_add_bit_flip_block_WR(int socket, uint32_t size, uint64_t offSet,
                                        bool persistent);

//Hash Mode
FSP_Response fsp_add_bit_flip_hash(int socket, uint32_t content_size, char* content, 
                                        uint8_t operation, bool persistent);
FSP_Response fsp_add_bit_flip_hash_write(int socket, uint32_t content_size, char* content,
                                        bool persistent);
FSP_Response fsp_add_bit_flip_hash_read(int socket, uint32_t content_size, char* content,
                                        bool persistent);
FSP_Response fsp_add_bit_flip_hash_WR(int socket, uint32_t content_size, char* content,
                                        bool persistent);

//Dedup Mode
FSP_Response fsp_add_bit_flip_dedup(int socket, uint32_t content_size, char* content, uint8_t operation, bool persistent);
FSP_Response fsp_add_bit_flip_dedup_write(int socket, uint32_t content_size, char* content, bool persistent);
FSP_Response fsp_add_bit_flip_dedup_read(int socket, uint32_t content_size, char* content, bool persistent);
FSP_Response fsp_add_bit_flip_dedup_WR(int socket, uint32_t content_size, char* content, bool persistent);

//Device Mode
FSP_Response fsp_add_bit_flip_device(int socket, uint8_t operation, bool persistent);
FSP_Response fsp_add_bit_flip_device_write(int socket, bool persistent);
FSP_Response fsp_add_bit_flip_device_read(int socket, bool persistent);
FSP_Response fsp_add_bit_flip_device_WR(int socket, bool persistent);

/********************************** Slow Disk ********************************************/

//Block Mode
FSP_Response fsp_add_slow_disk_block(int socket, uint32_t size, uint64_t offSet, 
                                            uint8_t operation, bool persistent, uint64_t time_ms);
FSP_Response fsp_add_slow_disk_block_write(int socket, uint32_t size, uint64_t offSet, bool persistent, uint64_t time_ms);
FSP_Response fsp_add_slow_disk_block_read(int socket, uint32_t size, uint64_t offSet, bool persistent, uint64_t time_ms);
FSP_Response fsp_add_slow_disk_block_WR(int socket, uint32_t size, uint64_t offSet, bool persistent, uint64_t time_ms);

//Hash Mode
FSP_Response fsp_add_slow_disk_hash(int socket, uint32_t content_size, char* content, uint8_t operation, bool persistent, uint64_t time_ms);
FSP_Response fsp_add_slow_disk_hash_write(int socket, uint32_t content_size, char* content, bool persistent, uint64_t time_ms);
FSP_Response fsp_add_slow_disk_hash_read(int socket, uint32_t content_size, char* content, bool persistent, uint64_t time_ms);
FSP_Response fsp_add_slow_disk_hash_WR(int socket, uint32_t content_size, char* content, bool persistent, uint64_t time_ms);

//Dedup Mode
FSP_Response fsp_add_slow_disk_dedup(int socket, uint32_t content_size, 
                                            char *content, uint8_t operation, 
                                            bool persistent, uint64_t time_ms);
FSP_Response fsp_add_slow_disk_dedup_write(int socket, uint32_t content_size, char *content, bool persistent, uint64_t time_ms);
FSP_Response fsp_add_slow_disk_dedup_read(int socket, uint32_t content_size, char *content, bool persistent, uint64_t time_ms);
FSP_Response fsp_add_slow_disk_dedup_WR(int socket, uint32_t content_size, char *content, bool persistent, uint64_t time_ms);

//Device Mode
FSP_Response fsp_add_slow_disk_device(int socket, uint8_t operation, bool persistent, uint64_t time_ms);
FSP_Response fsp_add_slow_disk_device_write(int socket, bool persistent, uint32_t time_ms);
FSP_Response fsp_add_slow_disk_device_read(int socket, bool persistent, uint32_t time_ms);
FSP_Response fsp_add_slow_disk_device_WR(int socket, bool persistent, uint32_t time_ms);

/********************************* Medium Error *******************************************/

//Block Mode
FSP_Response fsp_add_medium_error_block(int socket, uint32_t size, uint64_t offSet, uint8_t operation, bool persistent);
FSP_Response fsp_add_medium_error_block_write(int socket, uint32_t size, uint64_t offSet, bool persistent);
FSP_Response fsp_add_medium_error_block_read(int socket, uint32_t size, uint64_t offSet, bool persistent);
FSP_Response fsp_add_medium_error_block_WR(int socket, uint32_t size, uint64_t offSet, bool persistent);

//Hash Mode
FSP_Response fsp_add_medium_error_hash(int socket, uint32_t content_size, 
                                            char *content, uint8_t operation, bool persistent);
FSP_Response fsp_add_medium_error_hash_write(int socket, uint32_t content_size, char *content, bool persistent);
FSP_Response fsp_add_medium_error_hash_read(int socket, uint32_t content_size, char *content, bool persistent);
FSP_Response fsp_add_medium_error_hash_WR(int socket, uint32_t content_size, char *content, bool persistent);

//Dedup Mode
FSP_Response fsp_add_medium_error_dedup(int socket, uint32_t content_size, 
                                            char *content, uint8_t operation, bool persistent);
FSP_Response fsp_add_medium_error_dedup_write(int socket, uint32_t content_size, char *content, bool persistent);
FSP_Response fsp_add_medium_error_dedup_read(int socket, uint32_t content_size, char *content, bool persistent);
FSP_Response fsp_add_medium_error_dedup_WR(int socket, uint32_t content_size, char *content, bool persistent);

FSP_Response fsp_add_medium_error_device(int socket, uint8_t operation, bool persistent);
FSP_Response fsp_add_medium_error_device_write(int socket, bool persistent);
FSP_Response fsp_add_medium_error_device_read(int socket, bool persistent);
FSP_Response fsp_add_medium_error_device_WR(int socket, bool persistent);

/************************************* REMOVERS ********************************************/

FSP_Response fsp_remove_all_faults(int socket);

#endif