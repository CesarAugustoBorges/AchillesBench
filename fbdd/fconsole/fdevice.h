#define _GNU_SOURCE

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#ifndef F_DEVICE_HEADER
#define F_DEVICE_HEADER

#define MODE_PREFIX 0
#define MODE_APPEND 1

typedef struct F_Device {
    int socket;
    int fd;
    uint64_t offSet;
    uint64_t lastWrite;
    int mode;
    bool lastOpFailed;
} *F_Device;

F_Device f_new_device(int fd, int socket, bool isFile);
void f_free_device(F_Device d);

void f_close(F_Device d);
void f_open(F_Device device, char* path);
int f_write(F_Device d, void* buf, uint32_t size);
int f_write_block(F_Device d, void* buf, uint32_t size, uint64_t offSet);
int f_read(F_Device d, void* buf, uint32_t size);
int f_read_block(F_Device d, void* buf, uint32_t size, uint64_t offSet);
void f_setMode(F_Device d, int mode);
void f_setLastWrite(F_Device d, uint64_t lastWrite);
void f_setOffSet(F_Device d, uint64_t offSet);
bool f_last_operation_failed(F_Device d);
void f_free_Device(F_Device d);
void f_tests(F_Device dev, uint64_t disk_size,
             int n_w, int n_r, int n_bf, int n_hf);

#endif