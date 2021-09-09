#include "fsp_structs.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <pthread.h>

#include "../fbd_structs.h"

#define BUF_SIZE 1024

FSP_Response fsp_hash_to_fbd_hash(FSP_Request_Hash fsp,  FBD_Hash *fbd){
    switch(fsp->hash_type){
        case FBD_HASH_MD5:
            memcpy(fbd->md5, fsp->hash.md5, 17);
            break;
        case FBD_HASH_XXH3_128:
            fbd->xxh3_128.low64 = fsp->hash.xxh3_128.low64;
            fbd->xxh3_128.high64 = fsp->hash.xxh3_128.high64;
            break;
        default: 
            return FBD_STS_INVALID_HASH_TYPE;
    }
    return FBD_STS_OK;
}

FSP_Request fsp_recvRequest(int fd){
    int len = sizeof(struct fsp_request);
    FSP_Request request = (FSP_Request) malloc(sizeof(len));
    recv(fd, request, len, 0);
    return request;
}

int fsp_send_response(int fd, FSP_Response response){
    return send(fd, &response, sizeof(FSP_Response), 0);
}


FSP_Response handle_block_requests(FBD_Device dev, FSP_Request req){
    FSP_Request_Block rb = &(req->request_mode.block);
    switch (req->fault){
    case FBD_FAULT_BIT_FLIP:
        return fbd_add_bit_flip_block_fault(dev, rb->size, rb->offSet, req->operation, req->persistent);  
    case FBD_FAULT_SLOW_DISK:;
        uint64_t ms = ((uint64_t *) req->args)[0];
        return fbd_add_slow_disk_block_fault(dev, rb->size, rb->offSet, req->operation, req->persistent, ms);
    case FBD_FAULT_MEDIUM:
        return fbd_add_medium_error_block_fault(dev, rb->size, rb->offSet, req->operation, req->persistent);
    default:
        return FBD_STS_NOT_FOUND;
    }
}

FSP_Response handle_hash_requests(FBD_Device dev, FSP_Request req){
    FSP_Request_Hash rh = (FSP_Request_Hash) &(req->request_mode.hash);
        FBD_Hash fbd_hash;
    FSP_Response res;
    res = fsp_hash_to_fbd_hash(rh, &fbd_hash);
    if(res < 0)
        return res;
    switch (req->fault){
    case FBD_FAULT_BIT_FLIP:
        return fbd_add_bit_flip_hash_fault(dev, &fbd_hash, req->operation, req->persistent);
    case FBD_FAULT_SLOW_DISK:;
        uint64_t ms = ((uint64_t *) req->args)[0];
        return fbd_add_slow_disk_hash_fault(dev, &fbd_hash, req->operation, req->persistent, ms);
    case FBD_FAULT_MEDIUM:
        return fbd_add_medium_error_hash_fault(dev, &fbd_hash, req->operation, req->persistent);
    default:
        return FBD_STS_NOT_FOUND;
    }
}

FSP_Response handle_dedup_requests(FBD_Device dev, FSP_Request req){
    FSP_Request_Dedup rh =  &(req->request_mode.dedup);
        FBD_Hash fbd_hash;
    FSP_Response res;
    res = fsp_hash_to_fbd_hash(rh, &fbd_hash);
    if(res < 0)
        return res;
    switch (req->fault){
    case FBD_FAULT_BIT_FLIP:
        return fbd_add_bit_flip_dedup_fault(dev, &fbd_hash, req->operation, req->persistent);
    case FBD_FAULT_SLOW_DISK:;
        uint64_t ms = ((uint64_t *) req->args)[0];
        return fbd_add_slow_disk_dedup_fault(dev, &fbd_hash, req->operation, req->persistent, ms);
    case FBD_FAULT_MEDIUM:
        return fbd_add_medium_error_dedup_fault(dev, &fbd_hash, req->operation, req->persistent);
    default:
        return FBD_STS_NOT_FOUND;
    }
}

FSP_Response handle_device_requests(FBD_Device dev, FSP_Request req){
    switch (req->fault){
    case FBD_FAULT_BIT_FLIP:
        return fbd_add_bit_flip_block_fault(dev, dev->size, 0, req->operation, req->persistent);  
    case FBD_FAULT_SLOW_DISK:;
        uint64_t ms = ((uint64_t *) req->args)[0];
        return fbd_add_slow_disk_block_fault(dev, dev->size, 0, req->operation, req->persistent, ms);
    case FBD_FAULT_MEDIUM:
        return fbd_add_medium_error_block_fault(dev, dev->size, 0, req->operation, req->persistent);
    default:
        return FBD_STS_NOT_FOUND;
    }
}

FSP_Response handle_request(FBD_Device dev, FSP_Request req){
    printf(".............handling request............\n");
    fsp_print_request(req);
    switch (req->mode){
    case FBD_MODE_BLOCK:
        if(dev->user_settings->block_mode)
            return handle_block_requests(dev, req);
        else return FBD_STS_INVALID_MODE;
    case FBD_MODE_HASH:
        if(dev->user_settings->hash_mode)
            return handle_hash_requests(dev, req);
        else return FBD_STS_INVALID_MODE;
    case FBD_MODE_DEDUP:
        if(dev->user_settings->dedup_mode)
            return handle_dedup_requests(dev, req);
        else return FBD_STS_INVALID_MODE;
    case FBD_MODE_DEVICE:
        if(dev->user_settings->device_mode)
            return handle_device_requests(dev, req);
        else return FBD_STS_INVALID_MODE;
    case FBD_MODE_RESET_ALL:
        return fbd_remove_all_faults(dev);
    default:
        return FBD_STS_INVALID_MODE;
    }
}


void* fsp_startServer(void* device_ptr){
    FBD_Device device = (FBD_Device) device_ptr;
    
    FBD_Thread_Info thread_info = device->thread_info;
    pthread_mutex_lock(&thread_info->lock);
    while(!thread_info->bdus_started){
        pthread_cond_wait(&thread_info->cond, &thread_info->lock);
    }
    pthread_mutex_unlock(&thread_info->lock);

    //fbd_print_user_settings(device);

    int s, s2, len;
    unsigned int t;
    struct sockaddr_un local, remote;
    //char buf[BUF_SIZE] = {0};
    struct fsp_request request;
    FSP_Response response;

    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("Couldn't open socket");
        exit(1);
    }

    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, SOCK_PATH);
    unlink(local.sun_path);
    len = strlen(local.sun_path) + sizeof(local.sun_family);
    if (bind(s, (struct sockaddr *)&local, len) == -1) {
        perror("Couldn't bind socket");
        exit(1);
    }

    if (listen(s, 1) == -1) {
        perror("Socket couldn't listen");
        exit(1);
    }

    for(;;) {
        int n;
        //printf("Waiting for a connection...\n");
        t = sizeof(remote);
        if ((s2 = accept(s, (struct sockaddr *)&remote, &t)) == -1) {
            perror("Couldn't accept connection");
            exit(1);
        }

        //printf("Connection accepted.\n");
        do {
            n = recv(s2, &request, sizeof(struct fsp_request), 0);
            printf("n: %d\n", n);
            if (n < 0) {
                perror("recv");
            }
            if(n > 0){
                response = handle_request(device, &request);
                //fsp_print_request(request);
                fsp_print_response(response);
                fsp_send_response(s2, response);
            }
        } while (n > 0);
        if(n == 0){
            printf("closing connection\n");
            close(s2);
        } 
    }
    return 0;
}

void* fsp_startServer_Thread(void* device_path, pthread_t *thread_id){
    pthread_create(thread_id, NULL, fsp_startServer, device_path);
    return NULL;
}