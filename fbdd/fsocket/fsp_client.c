#include "fsp_client.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>


int fsp_send_request(int socket, FSP_Request request){
    /*int sizeof_request = fsp_sizeof_request(request);
    if(sizeof_request == -1) return sizeof_request;
    printf("request size: %d\n", sizeof_request);
    char buf[sizeof_request];*/
    /*if(fsp_request_to_buffer(request, buf) == -1){
        return -1;
    }*/
    //fsp_print_request(request);
    return send(socket, request, sizeof(struct fsp_request), 0);
}

FSP_Response fsp_recv_response(int fd, FSP_Request req){
    FSP_Response response;
    recv(fd, &response, sizeof(response), 0);
    //uint64_t *offSet = req->mode == FBD_MODE_BLOCK ? (void *) &((FSP_Request_Block) req->ptr)->offSet : NULL;
    //void* key = (req->mode == FBD_MODE_DEDUP ? (void*) ((FSP_Request_Dedup) req->ptr)->content : 
    //    (req->mode == FBD_MODE_HASH  ? (void*) ((FSP_Request_Hash) req->ptr)->content : NULL));
    //fsp_cache_update_response(req->mode, offSet ? offSet : key , req->fault, response);
    //printf("Has fault %s ? %s\n", fbd_fault_to_string(req->fault), 
        //fsp_cache_has_fault(offSet, key, FBD_FAULT_BIT_FLIP) ? "Yes" : "No");
    return response;
}

int fsp_socket(){
    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    if(s == -1) {
        perror("Couldn't open socket");
        exit(1);
    }
    return s;
}

void fsp_connect(int fd){
    struct sockaddr_un remote;
    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, SOCK_PATH);
    int len = strlen(remote.sun_path) + sizeof(remote.sun_family);
    if (connect(fd, (struct sockaddr *)&remote, len) == -1) {
        perror("Couldn't connect");
        exit(1);
    }
}

FSP_Response fsp_handle_send_request(int socket, FSP_Request request){
    if(fsp_send_request(socket, request) == -1){
        free(request);
        return FBD_STS_SEND_FAILED;
    }
    FSP_Response response = fsp_recv_response(socket, request);
    free(request);
    return response;
}


/******************************************* Cache **************************************************/
/*
FSP_Cache __fsp_cache;

void fsp_cache_init(){
    if(!__fsp_cache)
        __fsp_cache = fsp_new_cache();
    printf("fsp_cache: %p\n", __fsp_cache);
}

FSP_Cache fsp_cache_get(){
    return __fsp_cache;
}

GArray* fsp_new_faults_responses(){
    return g_array_sized_new(FALSE, TRUE, sizeof(FSP_Response), MAX_FAULTS);
}

void fsp_update_fault_response(GArray *faults_responses, uint8_t fault, FSP_Response res){
    FSP_Response last_response = g_array_index(faults_responses, FSP_Response, fault);
    // if there is a positive response in cache, it can't turn into a negative one
    if(res >= 0 || last_response <= 0){
        g_array_insert_val(faults_responses, fault, res);
    }
}


FSP_Cache fsp_new_cache(){
    FSP_Cache cache  = (FSP_Cache) malloc(sizeof(struct fsp_cache));
    cache->res_block = g_hash_table_new(g_int64_hash, g_int64_equal);
    cache->res_hash  = g_hash_table_new(g_str_hash, g_str_equal);
    cache->res_dedup = g_hash_table_new(g_str_hash, g_str_equal);
    cache->res_full_disk = fsp_new_faults_responses();
    return cache;
}

void fsp_cache_update_hash_table(GHashTable *hash_table, gpointer key, uint8_t fault, FSP_Response res){
    GArray *faults_responses = (GArray*) g_hash_table_lookup(hash_table, key);
    if(!faults_responses){
        faults_responses = fsp_new_faults_responses();
        if(!g_hash_table_insert(hash_table, key, faults_responses)){
            printf("ERROR: Failed to save in block cache!!!!\n");
        }
    }
    fsp_update_fault_response(faults_responses, fault, res);
}

GHashTable* fsp_cache_get_hash_table_by_mode(uint8_t mode){
    switch (mode){
    case FBD_MODE_BLOCK:
        return __fsp_cache->res_block;
    case FBD_MODE_DEDUP:
        return __fsp_cache->res_dedup;
    case FBD_MODE_HASH:
        return __fsp_cache->res_hash;
    default:
        return NULL;
    }
}

//TODO simplificar com o "fsp_cache_get_hash_table_by_mode"
void fsp_cache_update_response(uint8_t mode, void* key, uint8_t fault, FSP_Response res){
    if(!__fsp_cache)
        return;

    gpointer gkey = NULL;
    GHashTable *hash_table;
    switch (mode){
    case FBD_MODE_BLOCK:
        gkey = malloc(sizeof(uint64_t));
        *((uint64_t*) gkey) = *((uint64_t*) key);
        hash_table = __fsp_cache->res_block;
        break;
    case FBD_MODE_DEDUP:
        gkey = strdup((char*) key);
        hash_table = __fsp_cache->res_dedup;
        break;
    case FBD_MODE_HASH:
        gkey = strdup((char*) key);
        hash_table = __fsp_cache->res_hash;
        break;
    case FBD_MODE_DEVICE:
        fsp_update_fault_response(__fsp_cache->res_full_disk, fault, res);
        return;
    default:
        return;
    }
    fsp_cache_update_hash_table(hash_table, gkey, fault, res);
}

GArray* fsp_cache_get_responses(uint8_t mode, void* key){
    if(!__fsp_cache)
        return NULL;

    GArray* faults_responses = NULL;
    if(mode == FBD_MODE_DEVICE){
        faults_responses = __fsp_cache->res_full_disk;
    } else {
        GHashTable *hash_table = fsp_cache_get_hash_table_by_mode(mode);
        faults_responses = g_hash_table_lookup(hash_table, key);
    }
    return faults_responses;
}

FSP_Response fsp_cache_get_fault_response(uint8_t mode, void* key, uint8_t fault){
    if(!__fsp_cache)
        return FBD_STS_CACHE_NOT_FOUND;

    GArray* faults_responses = fsp_cache_get_responses(mode, key);
    
    if(faults_responses){
        FSP_Response response = g_array_index(faults_responses, FSP_Response, fault);
        return response;
    } else return FBD_STS_CACHE_NOT_FOUND;
}

bool fsp_cache_has_fault_by_mode(uint8_t mode, void* key, uint8_t fault){
    return fsp_cache_get_fault_response(mode, key, fault) > 0;
}

bool fsp_cache_has_fault(uint64_t *offSet, char* hash, uint8_t fault){
    bool found = false;
    if(offSet) 
        found = found || fsp_cache_has_fault_by_mode(FBD_MODE_BLOCK, offSet, fault);
    if(hash)
        found = found ||  fsp_cache_has_fault_by_mode(FBD_MODE_HASH, hash, fault) ||
            fsp_cache_has_fault_by_mode(FBD_MODE_DEDUP, hash, fault);
    found = found || fsp_cache_has_fault_by_mode(FBD_MODE_DEVICE, NULL, fault);
    return found;
}*/

/****************************************** Generic ************************************************/
FSP_Response fsp_add_generic_block(int socket, uint32_t size, uint64_t offSet, uint8_t operation, 
                                        int fault, bool persistent, void* extra, int extra_size){
     FSP_Request r = fsp_new_request_block(operation, fault, size, offSet, persistent, 
                                            extra, extra_size);
    return fsp_handle_send_request(socket, r);
}

FSP_Response fsp_add_generic_hash(int socket, uint32_t size, char* content, uint8_t operation, 
                                        int fault, bool persistent, uint8_t hash_type,
                                        void* extra, int extra_size){
     FSP_Request r = fsp_new_request_hash(operation, fault, size, content, persistent, 
                                            hash_type, extra, extra_size);

    return fsp_handle_send_request(socket, r);
}

FSP_Response fsp_add_generic_dedup(int socket, uint32_t size, char* content, uint8_t operation, 
                                        int fault, bool persistent, uint8_t hash_type,
                                        void* extra, int extra_size){
     FSP_Request r = fsp_new_request_dedup(operation, fault, size, content, persistent,
                                            hash_type, extra, extra_size);
    return fsp_handle_send_request(socket, r);
}


/****************************************** Bit Flip ************************************************/

FSP_Response fsp_add_bit_flip_block(int socket, uint32_t size, uint64_t offSet, uint8_t operation, bool persistent){
    FSP_Request r = fsp_new_request_block(operation, FBD_FAULT_BIT_FLIP, size, offSet, persistent, NULL, 0);
    return fsp_handle_send_request(socket, r);
}

FSP_Response fsp_add_bit_flip_hash(int socket, uint32_t content_size, char* content, uint8_t operation, bool persistent){
    FSP_Request r = fsp_new_request_hash(operation, FBD_FAULT_BIT_FLIP, content_size, content,
                                            persistent, FBD_HASH_XXH3_128, NULL, 0);
    return fsp_handle_send_request(socket, r);
}

FSP_Response fsp_add_bit_flip_dedup(int socket, uint32_t content_size, char* content, uint8_t operation, bool persistent){
    FSP_Request r = fsp_new_request_dedup(operation, FBD_FAULT_BIT_FLIP, content_size, content,
                                            persistent, FBD_HASH_XXH3_128, NULL, 0);
    return fsp_handle_send_request(socket, r);
}

FSP_Response fsp_add_bit_flip_device(int socket, uint8_t operation, bool persistent){
    FSP_Request r = fsp_new_request_device(operation, FBD_FAULT_BIT_FLIP, persistent, NULL, 0);
    return fsp_handle_send_request(socket, r);
}

FSP_Response fsp_add_bit_flip_block_write(int socket, uint32_t size, uint64_t offSet, bool persistent){
    return fsp_add_bit_flip_block(socket, size, offSet, FBD_OP_WRITE, persistent);
}

FSP_Response fsp_add_bit_flip_block_read(int socket, uint32_t size, uint64_t offSet, bool persistent){
    return fsp_add_bit_flip_block(socket, size, offSet, FBD_OP_READ, persistent);
}

FSP_Response fsp_add_bit_flip_block_WR(int socket, uint32_t size, uint64_t offSet, bool persistent){
    return fsp_add_bit_flip_block(socket, size, offSet, FBD_OP_WRITE_READ, persistent);
}

FSP_Response fsp_add_bit_flip_hash_write(int socket, uint32_t content_size, char* content,  bool persistent){
    return fsp_add_bit_flip_hash(socket, content_size, content, FBD_OP_WRITE, persistent);
}

FSP_Response fsp_add_bit_flip_hash_read(int socket, uint32_t content_size, char* content,  bool persistent){
    return fsp_add_bit_flip_hash(socket, content_size, content, FBD_OP_READ, persistent);
}

FSP_Response fsp_add_bit_flip_hash_WR(int socket, uint32_t content_size, char* content, bool persistent){
    return fsp_add_bit_flip_hash(socket, content_size, content, FBD_OP_WRITE_READ, persistent);
}

FSP_Response fsp_add_bit_flip_dedup_write(int socket, uint32_t content_size, char* content, bool persistent){
    return fsp_add_bit_flip_dedup(socket, content_size, content, FBD_OP_WRITE, persistent);
}

FSP_Response fsp_add_bit_flip_dedup_read(int socket, uint32_t content_size, char* content, bool persistent){
    return fsp_add_bit_flip_dedup(socket, content_size, content, FBD_OP_READ, persistent);
}

FSP_Response fsp_add_bit_flip_dedup_WR(int socket, uint32_t content_size, char* content, bool persistent){
    return fsp_add_bit_flip_dedup(socket, content_size, content, persistent, FBD_OP_WRITE_READ);
}

FSP_Response fsp_add_bit_flip_device_write(int socket, bool persistent){
    return fsp_add_bit_flip_device(socket, FBD_OP_WRITE, persistent);
}

FSP_Response fsp_add_bit_flip_device_read(int socket, bool persistent){
    return fsp_add_bit_flip_device(socket, FBD_OP_READ, persistent);
}

FSP_Response fsp_add_bit_flip_device_WR(int socket, bool persistent){
    return fsp_add_bit_flip_device(socket, FBD_OP_WRITE_READ, persistent);
}

/***************************************** Slow disk ************************************************/

FSP_Response fsp_add_slow_disk_block(int socket, uint32_t size, uint64_t offSet, 
                                            uint8_t operation, bool persistent, uint64_t time_ms){
    FSP_Request r = fsp_new_request_block(operation, FBD_FAULT_SLOW_DISK, size, offSet, 
                                                persistent, (void*) &time_ms, sizeof(uint64_t));
    return fsp_handle_send_request(socket, r);
}

FSP_Response fsp_add_slow_disk_hash(int socket, uint32_t content_size, 
                                            char *content, uint8_t operation, bool persistent, uint64_t time_ms){
    FSP_Request r = fsp_new_request_hash(operation, FBD_FAULT_SLOW_DISK, content_size, content, persistent,
                                                FBD_HASH_XXH3_128, &time_ms, sizeof(time_ms));
    return fsp_handle_send_request(socket, r);
}


FSP_Response fsp_add_slow_disk_dedup(int socket, uint32_t content_size, 
                                            char *content, uint8_t operation, 
                                            bool persistent, uint64_t time_ms){
    FSP_Request r = fsp_new_request_dedup(operation, FBD_FAULT_SLOW_DISK, content_size, content, persistent, 
                                                FBD_HASH_XXH3_128, &time_ms, sizeof(time_ms));
    return fsp_handle_send_request(socket, r);
}

FSP_Response fsp_add_slow_disk_device(int socket, uint8_t operation, bool persistent, uint64_t time_ms){
    FSP_Request r = fsp_new_request_device(operation, FBD_FAULT_SLOW_DISK, persistent, &time_ms, sizeof(time_ms));
    return fsp_handle_send_request(socket, r);
}

FSP_Response fsp_add_slow_disk_block_write(int socket, uint32_t size, uint64_t offSet, bool persistent, uint64_t time_ms){
    return fsp_add_slow_disk_block(socket, size, offSet, FBD_OP_WRITE, persistent, time_ms);
}

FSP_Response fsp_add_slow_disk_block_read(int socket, uint32_t size, uint64_t offSet, bool persistent, uint64_t time_ms){
    return fsp_add_slow_disk_block(socket, size, offSet, FBD_OP_READ, persistent, time_ms);
}

FSP_Response fsp_add_slow_disk_block_WR(int socket, uint32_t size, uint64_t offSet, bool persistent, uint64_t time_ms){
    return fsp_add_slow_disk_block(socket, size, offSet, FBD_OP_WRITE_READ, persistent, time_ms);
}

FSP_Response fsp_add_slow_disk_hash_write(int socket, uint32_t content_size, char *content, bool persistent, uint64_t time_ms){
    return fsp_add_slow_disk_hash(socket, content_size, content, FBD_OP_WRITE, persistent, time_ms);
}

FSP_Response fsp_add_slow_disk_hash_read(int socket, uint32_t content_size, char *content, bool persistent, uint64_t time_ms){
    return fsp_add_slow_disk_hash(socket, content_size, content, FBD_OP_READ, persistent, time_ms);
}

FSP_Response fsp_add_slow_disk_hash_WR(int socket, uint32_t content_size, char *content, bool persistent, uint64_t time_ms){
    return fsp_add_slow_disk_hash(socket, content_size, content, FBD_OP_WRITE_READ, persistent, time_ms);
}

FSP_Response fsp_add_slow_disk_dedup_write(int socket, uint32_t content_size, char *content, bool persistent, uint64_t time_ms){
    return fsp_add_slow_disk_dedup(socket, content_size, content, FBD_OP_WRITE, persistent, time_ms);
}

FSP_Response fsp_add_slow_disk_dedup_read(int socket, uint32_t content_size, char *content, bool persistent, uint64_t time_ms){
    return fsp_add_slow_disk_dedup(socket, content_size, content, FBD_OP_READ, persistent, time_ms);
}

FSP_Response fsp_add_slow_disk_dedup_WR(int socket, uint32_t content_size, char *content, bool persistent, uint64_t time_ms){
    return fsp_add_slow_disk_dedup(socket, content_size, content, FBD_OP_WRITE_READ, persistent, time_ms);
}

FSP_Response fsp_add_slow_disk_device_write(int socket, bool persistent, uint32_t time_ms){
    return fsp_add_slow_disk_device(socket, FBD_OP_WRITE, persistent, time_ms);
}

FSP_Response fsp_add_slow_disk_device_read(int socket, bool persistent, uint32_t time_ms){
    return fsp_add_slow_disk_device(socket, FBD_OP_READ, persistent, time_ms);
}

FSP_Response fsp_add_slow_disk_device_WR(int socket, bool persistent, uint32_t time_ms){
    return fsp_add_slow_disk_device(socket, FBD_OP_WRITE_READ, persistent, time_ms);
}

/***************************************** Medium error *********************************************/

FSP_Response fsp_add_medium_error_block(int socket, uint32_t size, uint64_t offSet, 
                                                    uint8_t operation, bool persistent){
    FSP_Request r = fsp_new_request_block(operation, FBD_FAULT_MEDIUM, size, offSet, 
                                                persistent, NULL, 0);
    return fsp_handle_send_request(socket, r);
}

FSP_Response fsp_add_medium_error_hash(int socket, uint32_t content_size, 
                                            char *content, uint8_t operation, bool persistent){
    FSP_Request r = fsp_new_request_hash(operation, FBD_FAULT_MEDIUM, content_size, content,
                                                persistent, FBD_HASH_XXH3_128, NULL, 0);
    return  fsp_handle_send_request(socket, r);
}

FSP_Response fsp_add_medium_error_dedup(int socket, uint32_t content_size, 
                                            char *content, uint8_t operation, bool persistent){
    FSP_Request r = fsp_new_request_dedup(operation, FBD_FAULT_MEDIUM, content_size, content, 
                                                persistent, FBD_HASH_XXH3_128, NULL, 0);
    return  fsp_handle_send_request(socket, r);
}

FSP_Response fsp_add_medium_error_device(int socket, uint8_t operation, bool persistent){
    FSP_Request r = fsp_new_request_device(operation, FBD_FAULT_MEDIUM, persistent, NULL, 0);
    return fsp_handle_send_request(socket, r);
}

FSP_Response fsp_add_medium_error_block_write(int socket, uint32_t size, uint64_t offSet, bool persistent){
    return fsp_add_medium_error_block(socket, size, offSet, FBD_OP_WRITE, persistent);
}

FSP_Response fsp_add_medium_error_block_read(int socket, uint32_t size, uint64_t offSet, bool persistent){
    return fsp_add_medium_error_block(socket, size, offSet, FBD_OP_READ, persistent);
}

FSP_Response fsp_add_medium_error_block_WR(int socket, uint32_t size, uint64_t offSet, bool persistent){
    return fsp_add_medium_error_block(socket, size, offSet, FBD_OP_WRITE_READ, persistent);
}

FSP_Response fsp_add_medium_error_hash_write(int socket, uint32_t content_size, char *content, bool persistent){
    return fsp_add_medium_error_hash(socket, content_size, content, FBD_OP_WRITE, persistent);
}

FSP_Response fsp_add_medium_error_hash_read(int socket, uint32_t content_size, char *content, bool persistent){
    return fsp_add_medium_error_hash(socket, content_size, content, FBD_OP_READ, persistent);
}

FSP_Response fsp_add_medium_error_hash_WR(int socket, uint32_t content_size, char *content, bool persistent){
    return fsp_add_medium_error_hash(socket, content_size, content, FBD_OP_WRITE_READ, persistent);
}

FSP_Response fsp_add_medium_error_dedup_write(int socket, uint32_t content_size, char *content, bool persistent){
    return fsp_add_medium_error_dedup(socket, content_size, content, FBD_OP_WRITE, persistent);
}

FSP_Response fsp_add_medium_error_dedup_read(int socket, uint32_t content_size, char *content, bool persistent){
    return fsp_add_medium_error_dedup(socket, content_size, content, FBD_OP_READ, persistent);
}

FSP_Response fsp_add_medium_error_dedup_WR(int socket, uint32_t content_size, char *content, bool persistent){
    return fsp_add_medium_error_dedup(socket, content_size, content, FBD_OP_WRITE_READ, persistent);
}

FSP_Response fsp_add_medium_error_device_write(int socket, bool persistent){
    return fsp_add_medium_error_device(socket, FBD_OP_WRITE, persistent);
}

FSP_Response fsp_add_medium_error_device_read(int socket, bool persistent){
    return fsp_add_medium_error_device(socket, FBD_OP_READ, persistent);
}

FSP_Response fsp_add_medium_error_device_WR(int socket, bool persistent){
    return fsp_add_medium_error_device(socket, FBD_OP_WRITE_READ, persistent);
}

FSP_Response fsp_remove_all_faults(int socket){
    FSP_Request req_ptr = malloc(sizeof(struct fsp_request));
    req_ptr->mode = FBD_MODE_RESET_ALL;
    req_ptr->operation = 0;
    req_ptr->fault = 0;
    req_ptr->args_size = 0;
    return fsp_handle_send_request(socket, req_ptr);
}