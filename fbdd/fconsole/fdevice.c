#include "fdevice.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fsp_client.h>


/**
 * Checks if a certain file descriptor is valid, the process exists with code 1  if it is invalid.
 * @param fd file descriptor
 */
void checkfd(int fd){
    if(fd < 0){
        fprintf(stderr,"F_Device ERROR: Invalid file descriptor, %d\n" , fd);
        exit(1); 
    }
}

void gen_str_random(char *s, const int len) {
    static const char alphanum[] =  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    for (int i = 0; i < len; ++i) {
        s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    s[len] = '\0';
}

uint64_t gen_number(int min, int max){
    return rand() % max + min;
}

int next_operation(int min, int max, int done_ops[], int size){
    int next = gen_number(min, max);
    for(int i = 0; i < size; i++){
        if(next == done_ops[i]){
            next = (next + 1) % max;
            i = -1; 
        }
    }
    return next;
}

void check_counter(int count_index, int counters[], int threshold, int done_ops[], int *done_ops_size){
    if(counters[count_index] >= threshold){
        done_ops[*done_ops_size] = count_index;
        (*done_ops_size)++;
    }
}

uint64_t gen_random_block(uint64_t disk_size, uint32_t block_size){
    uint64_t max = disk_size / block_size;
    return gen_number(0, max) * block_size;
}

#define WRITE_INDEX 0
#define READ_INDEX 1
#define BLOCK_FAULT_INDEX 2
#define HASH_FAULT_INDEX 3

#define BIT_FLIP 0
#define SLOW_DISK 1
#define MEDIUM_ERROR 2


void f_tests(F_Device dev, uint64_t disk_size,
             int n_w, int n_r, int n_bf, int n_hf){
    checkfd(dev->fd);
    const int block_size = 4096;
    if(disk_size % block_size != 0){
        printf("Disk size is not a multiple of %d bytes, no tests done\n", block_size);
        return;
    }
    time_t t;
    srand((unsigned) time(&t));
    int counters[] = {0, 0, 0, 0};
    int done_ops[] = {0, 0, 0, 0};
    int done_ops_size = 0;
    int max_done_ops_size = 4;
    char *buf = aligned_alloc(block_size, block_size);
    char *bufAux = aligned_alloc(block_size, block_size);
    check_counter(WRITE_INDEX, counters, n_w, done_ops, &done_ops_size);
    check_counter(READ_INDEX, counters, n_r, done_ops, &done_ops_size);
    check_counter(BLOCK_FAULT_INDEX, counters, n_bf, done_ops, &done_ops_size);
    check_counter(HASH_FAULT_INDEX, counters, n_hf, done_ops, &done_ops_size);

    int fault, op, r_sts, w_sts;
    while(done_ops_size < max_done_ops_size){
        int next = next_operation(0, max_done_ops_size, done_ops, done_ops_size);
        uint64_t next_offSet = gen_random_block(disk_size, block_size);
        dev->offSet = next_offSet;
        switch (next){
        case WRITE_INDEX:
            op = -1;
            counters[WRITE_INDEX]++;
            f_write(dev, buf, block_size);
            check_counter(WRITE_INDEX, counters, n_w, done_ops, &done_ops_size);
            break;
        case READ_INDEX:
            op = -1;
            counters[READ_INDEX]++;
            f_read(dev, buf, block_size);
            check_counter(READ_INDEX, counters, n_r, done_ops, &done_ops_size);
            break;
        case BLOCK_FAULT_INDEX:
            op = BLOCK_FAULT_INDEX;
            fault = gen_number(0, 3);
            gen_str_random(buf, block_size);
            switch(fault){
                case BIT_FLIP:
                    fsp_add_bit_flip_block_write(dev->socket, block_size, dev->offSet, true); break;
                case SLOW_DISK:
                    fsp_add_slow_disk_block_WR(dev->socket, block_size, dev->offSet, 10, true); break;  
                case MEDIUM_ERROR:
                    fsp_add_medium_error_block_WR(dev->socket, block_size, dev->offSet, true); break;
                default:
                    fprintf(stderr, "No fault found in block write"); break;
            }
            break;

        case HASH_FAULT_INDEX:
            op = HASH_FAULT_INDEX;
            fault = gen_number(0, 3);
            gen_str_random(buf, block_size);
            switch(fault){
                case BIT_FLIP:
                    fsp_add_bit_flip_hash_write(dev->socket, block_size, buf, true); break;
                case SLOW_DISK:
                    fsp_add_slow_disk_hash_WR(dev->socket, block_size, buf, 10, true); break;  
                case MEDIUM_ERROR:
                    fsp_add_medium_error_hash_write(dev->socket, block_size, buf, true); break;
                default:
                    fprintf(stderr, "No fault found in block write"); break;
            }
            break;
        default:
            printf("Unexpected operation, exiting tests\n");
            return;
            break;
        }

        if(op > 0){
            w_sts = f_write(dev, buf, block_size);
            dev->lastWrite = next_offSet;
            r_sts = f_read(dev, bufAux, block_size);

            switch(fault){
            case BIT_FLIP:
                if(strcmp(buf, bufAux) == 0){
                    fprintf(stderr, "Bit Flip on write not working properly?\n");
                }
                break;
            case SLOW_DISK:
                break;
            case MEDIUM_ERROR:
                if(w_sts >= 0){
                    fprintf(stderr, "Medium error not working properly, %d, %d?\n", w_sts, r_sts);
                    return;
                }
                break;
            }
            counters[op]++;
            counters[WRITE_INDEX]++;
            counters[READ_INDEX]++;
            check_counter(op, counters, op == BLOCK_FAULT_INDEX ? n_bf : n_hf,
                                        done_ops, &done_ops_size);
        }

        if(dev->offSet + block_size >= disk_size){
            done_ops_size = max_done_ops_size;
            printf("Out of memory to do more tests tests\n");
        }

    }
    printf("Operations dones: %d:%d:%d:%d\n", counters[0], counters[1], 
                counters[2], counters[3]);
}

 /**
  * Updates the pointers offSet and lastWrite according to mode and size, 
  * only 2 modes are supported (APPEND, PREFIX)
  * @param offSet pointer to offSet before update
  * @param lastWrite pointer to lastWrite before update
  * @param mode current mode of writing + reading
  * @param size the size of wroten content, it increments offSet if mode is APPEND 
  */ 
void updateOffSet_LastWrite(F_Device d, int size){
    if(d->mode == MODE_APPEND){
        if(d->offSet != d->lastWrite){
            d->lastWrite = d->offSet;  
        } 
        d->offSet += size;
    } 
    // It is PREFIX by default
    else {
        d->offSet = d->lastWrite;
    }
}

/**
 * Returns a new F_Device with the specified file descriptor, other fields are set to default values
 * @param fd file descriptor
 * @return a new F_Device
 */ 
F_Device f_new_device(int fd, int socket, bool isFile){
    F_Device d = (F_Device) malloc(sizeof(struct F_Device));
    d->socket = socket;
    d->fd = fd;
    d->offSet = d-> lastWrite = 0;
    d->mode = MODE_PREFIX;
    d->lastOpFailed = false;
    return d;
}  

void f_open(F_Device device, char* path){
    if(device->fd >= 0) close(device->fd);
    device->fd = open(path, O_RDWR | O_DIRECT | O_SYNC | O_CREAT);
    if(device->fd == -1){
        perror("Could not open file descriptor for device");
        f_free_device(device);
        exit(1);
    }    
}

int f_write(F_Device d, void* buf, uint32_t size){
    checkfd(d->fd);
    int w = 0;
    w = pwrite(d->fd, buf, size, d->offSet);
    if(w < 0) {
        d->lastOpFailed = false;
        return w;
    }
    d->lastOpFailed = true;
    updateOffSet_LastWrite(d, w);
    return w;
}

int f_write_block(F_Device d, void* buf, uint32_t size, uint64_t offSet){
    d->offSet = offSet;
    return f_write(d, buf, size);
}

int f_read(F_Device d, void* buf, uint32_t size){
    checkfd(d->fd);
    int r = 0;
    r = pread(d->fd, buf, size, d->lastWrite); 
    if(r < 0){
        d->lastOpFailed = true;
        return r;
    } 
    d->lastOpFailed = false;
    return r;
}

bool f_last_operation_failed(F_Device d){
    return d->lastOpFailed;
}

int f_read_block(F_Device d, void* buf, uint32_t size, uint64_t offSet){
    d->lastWrite = offSet;
    return f_read(d, buf, size);
}

void f_free_device(F_Device d){
    if(d != NULL)
        free(d);
}

void f_close(F_Device d){
    if(d->fd >= 0)
        close(d->fd);
}

void f_setMode(F_Device d, int mode){
    d->mode = mode;
}

void f_setOffSet(F_Device d, uint64_t offSet){
    d->offSet = offSet;
}

void f_setLastWrite(F_Device d, uint64_t lastWrite){
    d->lastWrite = lastWrite;
}