#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <getopt.h>

#include "../fsweeper/fsweeper.h"
#include "fdevice.h"
#include "../fsocket/fsp_structs.h"

#define MODE_PREFIX 0
#define MODE_APPEND 1

#define MAX_BUF_SIZE 8192

#define BLOCK_SIZE 4096

void print_help_message(){
    printf("Help not added yet\n");
}

int offSets_from_arg(char* arg, uint32_t *size, uint64_t *offSet){
    int scanned = sscanf(arg, "(%d:%lu)", size, offSet);
    return scanned == 2;
}

uint32_t arg_with_offSets(char* arg, char *buf, uint64_t *default_offSet){
    uint32_t size; uint64_t offSet;
    int scanned = sscanf(arg, "(%d:%lu)%s", &size, &offSet, buf);
    char *bufAux = buf;
    int len = strlen(arg);
    if(scanned == 3){
        *default_offSet = offSet;
        char *toReplicate = strdup(buf);
        int len = strlen(toReplicate);
        for(int i = offSet; i < offSet+size; i += len){
            int sizecpy = len + i >= offSet+size ? offSet+size - i : len;
            memcpy(bufAux, toReplicate, sizecpy);
            bufAux += sizecpy;
        }
        free(toReplicate);
        return size;
    } else {
        strcpy(buf, arg);
    }
    return len;
}

void arg_check_content(F_Device dev, char* arg, char *buf){
    char content[MAX_BUF_SIZE] = {0};
    int len = arg_with_offSets(arg, content, &(dev->lastWrite));
    bool resiliente = fw_sweep_block(dev->fd, buf, len, dev->offSet, content);
    printf("Block(%lu) is resiliente? : %s\n", dev->offSet, resiliente ? "Yes" : "No");
    /*
    if(sscanf(arg, "!=%s", content) == 1){
        int len = arg_with_offSets(content, content, &(dev->lastWrite));
        f_read(dev, buf, len);
        if(strcmp(content, buf) != 0){
            printf("Ok\n");
        } else {
            printf("NOT EXPECTED: Content is equal\n");
        }
    } else {
        if(!sscanf(arg, "==%s", content))
            strcpy(buf, arg);
        int len = arg_with_offSets(content, content, &(dev->lastWrite));
        f_read(dev, buf, len);
        if(strcmp(content, buf) == 0){
            printf("Ok\n");
        } else {
            printf("NOT EXPECTED: Content not equal\n");
        }
    }*/
}

int arg_with_offSet_timeMS(char* arg, uint32_t *size, uint64_t *offSet, uint64_t *time_ms){
    return sscanf(arg, "(%d:%lu)%lu", size, offSet, time_ms);
}

int arg_replicate_content(char *arg, char* buf, uint32_t *time_ms){
    int size = 0;
    char *replicate;
    if((time_ms == NULL && sscanf(optarg, "(%d)%s", &size, buf) == 2) ||
    (time_ms && sscanf(optarg, "%d(%d)%s", time_ms, &size, buf))){
        replicate = strdup(buf);
        int len = strlen(replicate);
        for(int i = 0; i < size; i += len){
            int cpylen = i+len >= size ? size-i : len;
            memcpy(buf, replicate, cpylen);
            buf += cpylen;
        }
        return size;
    } else {
        strcpy(buf, arg);
        return strlen(buf);
    }
}

bool arg_true_or_false(char *arg, bool default_bool){
    if(strcmp(arg, "true") == 0){
        return true;
    } else if (strcmp(arg, "false") == 0){
        return false;
    } else {
        return default_bool;
    }
}

static struct option long_options[] =
        {
            {"help",     no_argument,       0, 'h'},
            {"transient",required_argument, 0, 't'},
            {"not-transient",required_argument, 0, 'T'},
            //REQUIRES ONE OF THE FOLLOWING
            {"device",   required_argument, 0, 'd'}, //OR
            {"file",     required_argument, 0, 'f'},
            //Hash and Block Modes
            {"dedup",    no_argument,       0, 'D'},
            {"hash",     no_argument,       0, 'H'},
            {"block",    no_argument,       0, 'K'},
            //WRITE_BLOCK_MODES
            {"append",   no_argument,       0, 'a'},
            {"prefix",   no_argument,       0, 'p'},
            //INDEXES
            {"offSet",   required_argument, 0, 'o'},
            {"lastWrite",required_argument, 0, 'i'},
            //OPERATIONS
            {"write",    required_argument, 0, 'w'},
            {"read",     optional_argument, 0, 'r'},
            {"read_hash",required_argument, 0, 'l'},
            {"check",    optional_argument, 0, 'c'},
            {"error",    optional_argument, 0, 'e'},
            //FAULTS
            {"bit_w",    optional_argument, 0, 'b'},
            {"bit_r",    optional_argument, 0, 'B'},
            {"disk_w",   required_argument, 0, 's'},
            {"disk_r",   required_argument  , 0, 'S'},
            {"medium_w", optional_argument, 0, 'm'},
            {"medium_r", optional_argument, 0, 'M'},
            {NULL, 0, NULL, 0}
        };


int main(int argc, char** argv){
    int option;
    int index;

    opterr = 0;

    int socket = fsp_socket();
    int fd = -1, n;
    fsp_connect(socket);
    
    uint32_t size, len;
    uint64_t offSet, time_ms;
    //Doesn't mean it uses Hash Mode, means the content will be hashed
    bool using_hash = false; 
    // If not using_hash -> block mode
    // If using_hash and using_dedup are true -> Dedup mode
    // If using_hash is true and using_dedup is false -> Hash mode
    bool using_dedup = false;
    bool persistent = true;
    F_Device device = f_new_device(fd, socket, false);
    FSP_Response res = -1;
    
    char* buf = aligned_alloc(BLOCK_SIZE, BLOCK_SIZE);
    //fsp_cache_init();

    if(argc == 1){
        print_help_message();
        return 0;
    }

    while((option = getopt_long(argc, argv, "KHDd:f:apo:i:w:r:l:c:e:bBs:SmMtT", long_options, NULL)) != -1){
        switch(option){
            case 'w':
                len = arg_with_offSets(optarg, buf, &(device->offSet));
                f_write(device, buf, len);
                break;
            case 'r':
                if(optarg && offSets_from_arg(optarg, &size, &offSet)){
                    n = f_read_block(device, buf, size, offSet);
                } else {
                    n = f_read(device, buf, MAX_BUF_SIZE);
                }
                if(n < 0){
                        perror("Error on read");
                } else {
                    printf("Read (%d):", n);
                    printf("%s\n", buf);
                    //fsp_print_buffer_hexa(buf, n);               
                }
                break;
            //read_hash
            case 'l':;
                char hash[17] = {0};           
                if(offSets_from_arg(optarg, &size, &offSet)){
                    n = f_read_block(device, buf, size, offSet);
                } else {
                    n = f_read(device, buf, BLOCK_SIZE);
                }
                if(n < 0){
                        perror("Error on read");
                } else{
                    fsp_string_to_hash(buf, BLOCK_SIZE, hash);
                } printf("Hash (17): %s\n", hash);
                break;
            case 'c':
                arg_check_content(device, optarg, buf);
                break;
            case 'e':;
                bool error_expected = arg_true_or_false(optarg, true);
                bool error_ocurred = f_last_operation_failed(device);
                if(error_expected == error_ocurred) 
                    printf("Ok\n");
                else
                    printf("NOT EXPECTED: last operation failed? %d\n", error_ocurred);
                break;
            case 'd':
                f_open(device, optarg);
                using_hash = false;
                break;
            case 'f':;
                f_open(device, optarg);
                using_hash = true;
            case 'a':
                f_setMode(device, MODE_APPEND);
                break;
            case 'p':
                f_setMode(device, MODE_PREFIX);
                break;
            case 'o':
                f_setOffSet(device, atoi(optarg));
                break;
            case 'i':
                f_setLastWrite(device, atoi(optarg));
                break;
            case 'b':
                if(!optarg){
                    res = fsp_add_bit_flip_device_write(socket, persistent);
                    printf("b:%d\n", res);
                    break;
                }
                if(!using_hash && offSets_from_arg(optarg, &size, &offSet)){
                    res = fsp_add_bit_flip_block_write(socket, size, offSet, persistent);
                    printf("b:%d\n", res);
                } else if(using_hash){
                    int content_size = arg_replicate_content(optarg, buf, NULL);
                    if(using_dedup){
                        printf("sending dedup request\n");
			printf("%d-%s", content_size, buf);

                        res = fsp_add_bit_flip_dedup_write(socket, content_size, buf, persistent);
                    } else {
                        res = fsp_add_bit_flip_hash_write(socket, content_size, buf, persistent);
                    }
                    printf("b:%d\n", res);
                } 
                else { fprintf(stderr, "Wrong input for adding bit flips\n"); }                   
                break;
            case 'B':
                if(!optarg){
                    res = fsp_add_bit_flip_device_read(socket, persistent);
                    printf("B:%d\n", res);
                    break;
                }
                if(offSets_from_arg(optarg, &size, &offSet)){
                    res = fsp_add_bit_flip_block_read(socket, size, offSet, persistent);
                    printf("B:%d\n", res);
                } else if(using_hash){
                    int content_size = arg_replicate_content(optarg, buf, NULL);
                    if(using_dedup){
                        res = fsp_add_bit_flip_dedup_read(socket, content_size, buf, persistent);
                    } else {
                        res = fsp_add_bit_flip_hash_read(socket, content_size, buf, persistent);
                    }
                    printf("B:%d\n", res);
                } else { fprintf(stderr, "Wrong input for adding bit flips\n"); }
                break;
            case 's':
                if(!using_hash && arg_with_offSet_timeMS(optarg, &size, &offSet, &time_ms) == 3){
                    res = fsp_add_slow_disk_block_write(socket, size, offSet, persistent, time_ms);
                    printf("s:%d\n", res);
                } else if(using_hash){
                    uint32_t time_ms = 0;
                    int content_size = arg_replicate_content(optarg, buf, &time_ms);
                    if(time_ms == 0){
                        fprintf(stderr, "Wrong input for slow disk\n");
                    } else {
                        if(using_dedup){
                            res = fsp_add_slow_disk_dedup_write(socket, content_size, buf, persistent, time_ms);
                        } else {
                            res = fsp_add_slow_disk_hash_write(socket, content_size, buf, persistent, time_ms);
                        }
                        printf("S:%d\n", res);
                    }
                } else if(sscanf(optarg,"%lu", &time_ms) == 1){
                    res = fsp_add_slow_disk_device_write(socket, persistent, time_ms);
                    printf("s:%d\n", res);
                } else { fprintf(stderr, "Wrong input for slow disk\n"); }
                break;
            case 'S':
                if(sscanf(optarg,"%lu", &time_ms) == 1){
                    res = fsp_add_slow_disk_device_read(socket, persistent, time_ms);
                    printf("S:%d\n", res);
                    break;
                }
                if(!using_hash && arg_with_offSet_timeMS(optarg, &size, &offSet, &time_ms) == 3){
                    res = fsp_add_slow_disk_block_read(socket, size, offSet, persistent, time_ms);
                    printf("S:%d\n", res);
                } else if(using_hash){
                    uint32_t time_ms = 0;
                    int content_size = arg_replicate_content(optarg, buf, &time_ms);
                    if(time_ms == 0){
                        fprintf(stderr, "Wrong input for slow disk\n");
                    } else {
                        if(using_dedup){
                            res = fsp_add_slow_disk_dedup_read(socket, content_size, buf, persistent, time_ms);
                        } else {
                            res = fsp_add_slow_disk_hash_read(socket, content_size, buf, persistent, time_ms);
                        }
                        printf("S:%d\n", res);
                    }
                } else if(sscanf(optarg,"%lu", &time_ms) == 1){
                    res = fsp_add_slow_disk_device_write(socket, persistent, time_ms);
                    printf("s:%d\n", res);
                } else { fprintf(stderr, "Wrong input for slow disk\n"); }
                break;
            case 'm':
                if(!optarg){
                    res = fsp_add_medium_error_device_write(socket, persistent);
                    printf("m:%d\n", res);
                    break;
                }
                if(!using_hash && offSets_from_arg(optarg, &size, &offSet)){
                    res = fsp_add_medium_error_block_write(socket, size, offSet, persistent);
                    printf("m:%d\n", res);
                } else if(using_hash){
                    int content_size = arg_replicate_content(optarg, buf, NULL);
                    if(using_dedup){
                        res = fsp_add_medium_error_dedup_write(socket, content_size, buf, persistent);
                    } else {
                        res = fsp_add_medium_error_hash_write(socket, content_size, buf, persistent);
                    }
                    printf("m:%d\n", res);
                } else { fprintf(stderr, "Wrong input for adding medium error\n");}
                break;
            case 'M':
                if(!optarg){
                    res = fsp_add_medium_error_device_read(socket, persistent);
                    printf("M:%d\n", res);
                    break;
                }
                if(!using_hash && offSets_from_arg(optarg, &size, &offSet)){
                    res = fsp_add_medium_error_block_read(socket, size, offSet, persistent);
                    printf("M:%d\n", res);
                } else if(using_hash){
                    int content_size = arg_replicate_content(optarg, buf, NULL);
                    if(using_dedup){
                        res = fsp_add_medium_error_dedup_read(socket, content_size, buf, persistent);
                    } else {
                        res = fsp_add_medium_error_hash_read(socket, content_size, buf, persistent);
                    }
                    printf("M:%d\n", res);
                } else { fprintf(stderr, "Wrong input for adding medium error\n");}
                break;
            case 'h':
                print_help_message();
                break;
            case 'K':
                using_hash = false;
                using_dedup = false;
                break;
            case 'H':
                using_hash = true;
                using_dedup = false;
                break;
            case 'D':
                using_hash = true;
                using_dedup = true;
                break;
            case 't':;
                /*int n_w, n_r, n_bf, n_hf;
                if(sscanf(optarg, "%d:%d:%d:%d", &n_w, &n_r, &n_bf, &n_hf) == 4){
                    f_tests(device, 1 << 30, n_w, n_r, n_bf, n_hf);
                } else {
                    fprintf(stderr, "Wrong input for tests");
                }
                break;*/
                persistent = false;
                break;
            case 'T':
                persistent = true;
                break;
            case '?':
                if(optopt == 'w' || optopt == 'c' || optopt == 'f' || optopt == 'd'){
                    fprintf(stderr, "Missing argument for option '-%c'\n", optopt);
                } else if (isprint(optopt)){
                    fprintf(stderr, "Unknown option '-%c'\n", optopt);
                } else {
                    fprintf(stderr, "Unknown caracther '-%c'\n", optopt);
                }
                exit(0);
            default:
                fprintf(stderr, "Unknow option '-%c'\n", optopt);
                exit(0);
        }
    }

    for(index = optind; index < argc; index++){
        fprintf(stderr, "Ignoring argument \"%s\"\n", argv[index]);
    }
    printf("Closing\n");
    free(buf);
    f_close(device);
    f_free_device(device);

    return 0;
}
