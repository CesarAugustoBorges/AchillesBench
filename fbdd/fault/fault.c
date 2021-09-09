#include <time.h>
#include <errno.h>
#include <stdio.h>

#include "../fbd_structs.h"
#include "fault.h"

int msleep(long tms){
    struct timespec ts;
    int ret;
 
    ts.tv_sec = tms / 1000;
    ts.tv_nsec = (tms % 1000) * 1000000;
 
    do {
        ret = nanosleep(&ts, &ts);
    } while (ret && errno == EINTR);
 
    return ret;
}

int fl_inject_bit_flip_fault_buffer(char* buffer, uint32_t size){
    printf("------------- Injecting Bit Blip ---------------\n");
    printf("BEFORE FLIP: %c\n", buffer[0]);
    buffer[0] = buffer[0]^1;
    printf("AFTER FLIP : %c\n", buffer[0]);
    printf("Injected\n");
    return FBD_STS_OK;
}

int fl_inject_slow_disk_fault(uint64_t ms){
    printf("------------- Injecting Slow Disk --------------\n");
    printf("Sleep time %lu ms\n", ms);
    msleep(ms);
    printf("Injected\n");
    return FBD_STS_OK;
}

int fl_inject_medium_disk_fault(){
    printf("--------- Injecting Medium Disk Error ----------\n");
    printf("Injected\n");
    return FBD_STS_MEDIUM_ERROR;
}
