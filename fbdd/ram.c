#include <bdus.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>


static int device_read(char *buffer, uint64_t offset, uint32_t size, struct bdus_dev *dev){
    printf("w: %ld-%ld\n", offset, offset+size);
    memcpy(buffer, (char *)dev->user_data + offset, size);
    return 0;
}

static int device_write(const char *buffer, uint64_t offset, uint32_t size,struct bdus_dev *dev){
    printf("r: %ld-%ld\n", offset, offset+size);
    memcpy((char *)dev->user_data + offset , buffer, size );
    return 0;
}

static const struct bdus_ops device_ops = {
    .read  = device_read,
    .write = device_write,
};

static const struct bdus_attrs device_attrs = {
    .size               = 1 << 30,
    .logical_block_size = 4096,
    .dont_daemonize = true,
};

int main(void){
    void *buffer = malloc(device_attrs.size);

    if (!buffer)
        return 1;

    bool success = bdus_run(&device_ops, &device_attrs, buffer);    

    if(!success){
        fprintf(stderr, "ERROR: \"%s\"", bdus_get_error_message());
    }
    
    free(buffer);
    return success ? 0 : 1;
}