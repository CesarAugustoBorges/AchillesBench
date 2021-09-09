#include "fsp_structs.h"
#include "../fbd_defines.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

void fsp_print_buffer_hexa(char * buf, int size){
    for(int i = 0; i < size; i++){
        printf("%#x", buf[i]);
        if(i != size -1){
            printf(", ");
        }
    }
    printf("\n");
}

void fsp_string_to_hash(char *string, uint32_t string_size, char* hash_out){
    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, (const unsigned char*) string, (size_t) string_size);
    MD5_Final((unsigned char*) hash_out, &ctx);
}

void fsp_set_request_args(FSP_Request r, void* args, uint32_t args_size){
    r->args_size = args_size;
    if(args_size < 64)
        r->args[args_size] = '\0';
    memcpy(r->args, args, args_size);
}

FSP_Request fsp_new_request(uint8_t operation, uint8_t fault, bool persistent){
    FSP_Request request = (FSP_Request) malloc(sizeof(struct fsp_request));
    request->operation = operation;
    request->fault = fault;
    request->persistent = persistent;
    bzero(&request->request_mode, sizeof(union fsp_request_mode));
    return request;
}

XXH3_state_t *__xxh3_128_state;
FSP_Request_Hash fsp_new_request_hash_ptr(FSP_Request req, uint32_t content_size, 
                                        char *content, uint8_t hash_type){
    FSP_Request_Hash req_hash = &req->request_mode.hash;
    req_hash->hash_type = hash_type;
    if(hash_type == FBD_HASH_MD5){
        char hash[MD5_DIGEST_LENGTH + 1];
        hash[MD5_DIGEST_LENGTH] = '\0';
        fsp_string_to_hash(content, content_size, hash);

        //Variavel apenas para print
        //char print_content[content_size + 1];
        //memcpy(print_content, content, content_size);
        //printf("Hash: %s, content: %s\n", hash, print_content);
        memcpy(req_hash->hash.md5, hash, sizeof(union fsp_hash));
    } else if(hash_type == FBD_HASH_XXH3_128){
        //printf("content size: %d, (%d) content:%s\n", content_size, strlen(content), content);
        if(!__xxh3_128_state) __xxh3_128_state = XXH3_createState();
        if (XXH3_128bits_reset(__xxh3_128_state) == XXH_ERROR) abort();
        if (XXH3_128bits_update(__xxh3_128_state, content, content_size) == XXH_ERROR) abort();
        req_hash->hash.xxh3_128 = XXH3_128bits_digest(__xxh3_128_state);
    }
    return req_hash;
}

FSP_Request fsp_new_request_hash(uint8_t operation, uint8_t fault, uint32_t content_size, 
                                        char *content, bool persistent, uint8_t hash_type,
                                        void *args, uint32_t args_size){
    FSP_Request request = fsp_new_request(operation, fault, persistent);
    request->mode = FBD_MODE_HASH;
    fsp_new_request_hash_ptr(request, content_size, content, hash_type);
    fsp_set_request_args(request, args, args_size);
    return request;
}

void fsp_new_request_block_ptr(FSP_Request request, uint32_t size, uint64_t offSet){
    request->mode = FBD_MODE_BLOCK;
    FSP_Request_Block req_block = &request->request_mode.block;
    req_block->size = size;
    req_block->offSet = offSet;
}

FSP_Request fsp_new_request_dedup(uint8_t operation, uint8_t fault, uint32_t content_size,
                                        char *content, bool persistent, uint8_t hash_type, 
                                        void* args, uint32_t args_size){
    FSP_Request request = fsp_new_request(operation, fault, persistent);
    request->mode = FBD_MODE_DEDUP;
    // dedup can use hash ptr since the information is the same
    fsp_new_request_hash_ptr(request, content_size, content, hash_type);
    fsp_set_request_args(request, args, args_size);
    return request;
}


FSP_Request fsp_new_request_block(uint8_t operation, uint8_t fault, uint32_t size, 
                                        uint64_t offSet, bool persistent, 
                                        void* args, uint32_t args_size){
    FSP_Request request = fsp_new_request(operation, fault, persistent);
    fsp_new_request_block_ptr(request, size, offSet);
    fsp_set_request_args(request, args, args_size);
    return request;
}

FSP_Request fsp_new_request_device(uint8_t operation, uint8_t fault, bool persistent, 
                                                    void* args, uint32_t args_size){
    FSP_Request request = fsp_new_request(operation, fault, persistent);
    request->mode = FBD_MODE_DEVICE;
    fsp_set_request_args(request, args, args_size);
    return request;
}


void fsp_print_request(FSP_Request request){
    printf("-------------------Request-------------------\n");
    printf("request fault: %s\n", fbd_fault_to_string(request->fault));
    printf("operation: %s\n", fbd_operation_to_string(request->operation));
    printf("mode: %s\n", fbd_mode_to_string(request->mode));
    printf("persistent? %s\n", request->persistent ? "Yes" : "No");
    /*if(request->mode == FBD_MODE_BLOCK){
        FSP_Request_Block rb = request->request_mode.block;
        //printf("size: %d, offSet: %lu\n", rb->size, rb->offSet);
        
    } else if(request->mode == FBD_MODE_HASH){
        FSP_Request_Hash rh = request->request_mode.hash;
        //printf("hash: \"%s\"\n", rh->hash.md5);
    } else if(request->mode == FBD_MODE_DEDUP){
        FSP_Request_Dedup rd = request->request_mode.dedup;
        //printf("hash: \"%s\"\n", rd->size, rd->hash.md5);
    } else if(request->mode != FBD_MODE_DEVICE && request->mode != FBD_MODE_RESET_ALL){
        printf("WARNING: MODE NOR FOUND\n");        
    }*/
    if(request->args_size > 0) printf("args: ");
        for(int i = 0; i < request->args_size; i++){
            printf("%x", request->args[i]);
        }
    if(request->args_size > 0) printf("\n");
    printf("---------------------------------------------\n");
}


void fsp_print_response(FSP_Response response){
    printf("response status: %d\n", response);
}

/*
int fsp_request_to_buffer(FSP_Request r, char *buf){
    int len = sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(r->persistent);
    memcpy(buf, r, len);
    buf += len;
    if(r->mode == FBD_MODE_BLOCK){
        FSP_Request_Block rb = (FSP_Request_Block) r->ptr;
        memcpy(buf, &rb->size, sizeof(uint32_t));
        buf += sizeof(uint32_t);
        memcpy(buf, &rb->offSet, sizeof(uint64_t));
        buf += sizeof(uint64_t);
    } else if(r->mode == FBD_MODE_HASH){
        FSP_Request_Hash rh = (FSP_Request_Hash) r->ptr;
        len = sizeof(uint32_t);
        memcpy(buf, rh, len);
        buf += len;
        memcpy(buf, rh->content, rh->size);
        buf += rh->size;
    } else if(r->mode == FBD_MODE_DEDUP){
        FSP_Request_Dedup rd = (FSP_Request_Dedup) r->ptr;
        len = sizeof(uint32_t);
        memcpy(buf, rd, len);
        buf += len;
        memcpy(buf, rd->content, rd->size);
        buf += rd->size;
    } else if(r->mode == FBD_MODE_RESET_ALL){
        return 0;
    } else if(r->mode != FBD_MODE_DEVICE){
        return -1;
    }
    memcpy(buf, &r->args_size, sizeof(uint32_t));
    buf += sizeof(uint32_t);
    if(r->args_size > 0){
        memcpy(buf, r->args, r->args_size);
    } else {
        r->args = NULL;
    }
    return 0;
}*/

/*
int fsp_buffer_to_request(char *buf, int size, FSP_Request r){
    int len = sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(r->persistent);
    memcpy(r, buf, len);
    size -= len;
    buf += len;
    if(r->mode == FBD_MODE_RESET_ALL){
        return 0;
    }

    if(len >= size) return -1;
    if(r->ptr){
        free(r->ptr);
        r->ptr = NULL;
    }
    if(r->mode == FBD_MODE_HASH){
        FSP_Request_Hash rh = (FSP_Request_Hash) malloc(sizeof(struct fsp_request_hash));
        len = sizeof(uint32_t);
        memcpy(&rh->size, buf, len);
        size -= len;
        buf += len;
        rh->content =  (char*) malloc(rh->size);
        memcpy(rh->content, buf, rh->size); 
        r->ptr = rh;
        buf += rh->size;
        size -= rh->size;
    } else if(r->mode == FBD_MODE_BLOCK){
        FSP_Request_Block rb = (FSP_Request_Block) malloc(sizeof(struct fsp_request_block));
        memcpy(&rb->size, buf, sizeof(uint32_t));
        buf += sizeof(uint32_t);
        size -= sizeof(uint32_t);
        memcpy(&rb->offSet, buf, sizeof(uint64_t));
        buf += sizeof(uint64_t);
        size -= sizeof(uint64_t);
        r->ptr = rb;
    } else if(r->mode == FBD_MODE_DEDUP){
        FSP_Request_Dedup rd = (FSP_Request_Dedup) malloc(sizeof(struct fsp_request_block));
        len = sizeof(uint32_t);
        memcpy(&rd->size, buf, len);
        size -= len;
        buf += len;
        rd->content =  (char*) malloc(rd->size);
        memcpy(rd->content, buf, rd->size); 
        r->ptr = rd;
        buf += rd->size;
        size -= rd->size;
    } else if(r->mode != FBD_MODE_DEVICE){
        return -1;
    }
    memcpy(&r->args_size, buf, sizeof(uint32_t));
    buf += sizeof(uint32_t);
    size -= sizeof(uint32_t);
    if(r->args_size > 0){
        r->args = malloc(r->args_size);
        memcpy(r->args, buf, size);
        size -= r->args_size;
    } else {
        r->args = NULL;
    }
    printf("buffer to request size: %d\n", size);
    return size == 0 ? 0 : -1;
}*/

/*
int fsp_sizeof_request(FSP_Request r){
    int size = 0;
    size += sizeof(r->operation) + sizeof(r->fault) + sizeof(r->mode) +
            sizeof(r->persistent) + sizeof(r->args_size) + r->args_size;
    if(r->mode == FBD_MODE_BLOCK){
        FSP_Request_Block rb = (FSP_Request_Block) r->ptr;
        size += sizeof(rb->size) + sizeof(rb->offSet);
    } else if(r->mode == FBD_MODE_HASH){
        FSP_Request_Hash rh = (FSP_Request_Hash) r->ptr;
        size += sizeof(rh->size) + rh->size;
    } else if(r->mode == FBD_MODE_DEDUP){
        FSP_Request_Dedup rd = (FSP_Request_Dedup) r->ptr;
        size += sizeof(rd->size) + rd->size;
    } else if(r->mode == FBD_MODE_RESET_ALL){
        return size;
    } else if(r->mode != FBD_MODE_DEVICE) {
        return -1;
    }
    return size;
}*/


/*void fsp_cache_update_block_response(FSP_Cache cache, uint64_t offSet, uint8_t fault, FSP_Response res){
    GArray *faults_responses = (GArray*) g_hash_table_lookup(cache->res_block, &offSet);
    if(!faults_responses){
        uint64_t *offSet_ptr = (uint64_t*) malloc(sizeof(uint64_t)); 
        *offSet_ptr = offSet;
        faults_responses = fsp_new_faults_responses();
        if(!g_hash_table_insert(cache->res_block, offSet_ptr, faults_responses)){
            printf("ERROR: Failed to save in block cache!!!!\n");
        }
    } 
    fsp_update_fault_response(faults_responses, fault, res);
}

void fsp_cache_update_hash(GHashTable *hash_table, char* hash, uint8_t fault, FSP_Response res){
    GArray *faults_responses = (GArray*) g_hash_table_lookup(hash_table, hash);
    if(!faults_responses){
        char *hash_dup = strdup(hash);
        faults_responses = fsp_new_faults_responses();
        if(!g_hash_table_insert(hash_table, hash_dup, faults_responses)){
            printf("ERROR: Failed to save in hash cache!!!!\n");
        }
    } 
    fsp_update_fault_response(faults_responses, fault, res);
}

void fsp_cache_update_hash_response(FSP_Cache cache, char* hash, uint8_t fault, FSP_Response res){
    fsp_cache_update_hash(cache->res_hash, hash, fault, res);
}
void fsp_cache_update_dedup_response(FSP_Cache cache, char* hash, uint8_t fault, FSP_Response res){
    fsp_cache_update_hash(cache->res_dedup, hash, fault, res);
}
void fsp_cache_update_full_disk_response(FSP_Cache cache, uint8_t fault, FSP_Response res){
    fsp_update_fault_response(cache->res_full_disk, fault, res);
}*/

