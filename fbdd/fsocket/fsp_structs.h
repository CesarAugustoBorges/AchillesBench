#include <stdint.h>
#include <stdio.h>
#include <openssl/md5.h>
#include <glib.h>
#include <gmodule.h>
#include <stdbool.h>
#include <xxhash.h>

#include <fbd_defines.h>

#ifndef FAULTY_SOCKET_PROTOCOL_STRUCTS_HEADER
#define FAULTY_SOCKET_PROTOCOL_STRUCTS_HEADER

#define SOCK_PATH "/var/lib/fsocket/fault_injection_socket"

typedef struct fsp_request_block {
    uint32_t size;
    uint64_t offSet;
} *FSP_Request_Block;

typedef union fsp_hash {
    char md5[17];
    XXH128_hash_t xxh3_128;
} *FSP_Hash;

typedef struct fsp_request_hash {
    uint8_t hash_type;
    union fsp_hash hash;
} *FSP_Request_Hash;

typedef FSP_Request_Hash FSP_Request_Dedup; 

union fsp_request_mode {
    struct fsp_request_block block;
    struct fsp_request_hash hash;
    struct fsp_request_hash dedup;
};

typedef struct fsp_request {
    uint8_t operation;
    uint8_t fault;
    uint8_t mode;
    bool persistent;
    union fsp_request_mode request_mode;
    uint32_t args_size;
    char args[64];
} *FSP_Request;

typedef int8_t FSP_Response;

void fsp_print_buffer_hexa(char * buf, int size);

void fsp_string_to_hash(char *string, uint32_t string_size, char* hash_out);

FSP_Request fsp_new_request_block(uint8_t operation, uint8_t fault, uint32_t size, 
                                        uint64_t offSet, bool persistent, 
                                        void* args, uint32_t args_size);
FSP_Request fsp_new_request_hash(uint8_t operation, uint8_t fault, uint32_t content_size, 
                                        char *content, bool persistent, uint8_t hash_type,
                                        void *args, uint32_t args_size);
FSP_Request fsp_new_request_dedup(uint8_t operation, uint8_t fault, uint32_t content_size,
                                        char *content, bool persistent, uint8_t hash_type,
                                        void* args, uint32_t args_size);
FSP_Request fsp_new_request_device(uint8_t operation, uint8_t fault, bool persistent, 
                                                    void* args, uint32_t args_size);


/*
int fsp_request_to_buffer(FSP_Request r, char *buf);
int fsp_buffer_to_request(char *buf, int size, FSP_Request r);
int fsp_sizeof_request(FSP_Request r);
*/
void fsp_print_request(FSP_Request request);
void fsp_print_response(FSP_Response response);

#endif