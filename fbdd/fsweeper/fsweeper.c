#include <unistd.h>
#include <string.h>

#include "fsweeper.h"

bool fw_sweep_block(int fd, char* buf, int size, uint64_t offSet, char* expected_content){
    int r = 0;
    char hash[MD5_DIGEST_LENGTH + 1];
    fsp_string_to_hash(expected_content, size, hash);
    strcpy(buf, expected_content);
    r = pread(fd, buf, size, offSet);
    printf("size: %d, offSet %lu, r: %d\n", size, offSet, r);
    perror("");
    if(/*fsp_cache_has_fault(&offSet, hash, FBD_FAULT_MEDIUM) &&*/ r < 0){
        return false;
    }
    //printf("has fault?: %d\n", fsp_cache_has_fault(&offSet, hash, FBD_FAULT_BIT_FLIP));
    printf("strcmp: %d\n", strcmp(expected_content, buf));
    if(/*fsp_cache_has_fault(&offSet, hash, FBD_FAULT_BIT_FLIP) &&*/ strcmp(expected_content, buf) != 0){
        return false;
    }
    return true;
}