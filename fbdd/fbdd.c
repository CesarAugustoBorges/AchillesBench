/* -------------------------------------------------------------------------- */

// A device that mirrors another block device.

// Note that this driver can both create a new device (by running it with only
// the path to the underlying as an argument) or replace an existing device's
// driver (by also specifying the index or path of that device).

// Compile with:
//     cc loop.c -lbdus -o loop

/* -------------------------------------------------------------------------- */

#define _FILE_OFFSET_BITS 64

#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <bdus.h>

#include <errno.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>

#include "fbd_structs.h"
#include "./fsocket/fsp_server.h"

//Global variables
FBD_Device device;



/* -------------------------------------------------------------------------- */

static int device_initalize(struct bdus_dev *dev){
    device->path = strdup(dev->path);
    device->index = dev->index;
    device->size = dev->attrs->size;
    device->logical_block_size = dev->attrs->logical_block_size;
    //awakes server thread here
    FBD_Thread_Info thread_info = device->thread_info;
    pthread_mutex_lock(&thread_info->lock);
    thread_info->bdus_started = true;
    pthread_cond_signal(&thread_info->cond);
    pthread_mutex_unlock(&thread_info->lock);
    return 0;
}

static int device_read(
    char *buffer, uint64_t offset, uint32_t size,
    struct bdus_dev *dev){   
    char* rd_buf = (char* ) buffer;
    FBD_Device device = (FBD_Device) dev->user_data;
    int fd = device->fd;
    uint32_t size_aux = size;
    uint32_t block_size = 4096;//device->logical_block_size;
    uint32_t read_index = 0;
    uint64_t offset_aux = offset;

    //printf("read -> size:%d, offSet:%lu\n", size, offset);
    // read requested data from underlying device
    while (size > 0)
    {
        ssize_t res = pread(fd, buffer, (size_t)size, (off_t)offset);
        if (res < 0)
        {
            // read failed, retry if interrupted, fail otherwise

            if (errno != EINTR)
                return errno; // not interrupted, return errno
        }
        else if (res == 0)
        {
            // end-of-file, should not happen

            return EIO;
        }
        else
        {
            // successfully read some data

            buffer += res;
            offset += (uint64_t)res;
            size   -= (uint32_t)res;
        }
    }

    while(read_index < size_aux){
        uint32_t current_OffSet = offset_aux + read_index;
        //rd_buf = (char* ) buffer + current_OffSet;
        //char print_block[4097] = {0};
        //memcpy(print_block, rd_buf, block_size);
        //char hash[17] = {0};
        //fsp_string_to_hash(rd_buf, block_size, hash);

        int status = fbd_check_and_inject_read_fault(device, rd_buf, block_size, current_OffSet);
        if(status == FBD_STS_MEDIUM_ERROR) {
            printf("Returning ENOMEDIUM\n");
            return ENOMEDIUM;                                                                                                                                                                                                       ENOMEDIUM;
        } 
        read_index += block_size;
        rd_buf += block_size;
    }

    // success

    return 0;
}

void print_buffer_pretty(const char* buffer, uint32_t size){
    char last = buffer[0];
    for(int i = 0; i < size; i++){
        if(last != buffer[i])
            last = buffer[i];
        printf("%c", last);
        int r;
        for(r = 1; r+i < size && buffer[r+i] == last; r++);
        if(r > 1){
            printf("(%d)", r+1);
            i += r;
        }
    }
    printf("\n");
}

static int device_write(const char *buffer, uint64_t offset, uint32_t size,struct bdus_dev *dev){
    FBD_Device device = (FBD_Device) dev->user_data;
    int fd = device->fd;
    uint32_t write_index = 0;
    uint32_t block_size = 4096;//device->logical_block_size;
    char *wr_buf = (char *) buffer;
    while(write_index < size){
        uint64_t current_OffSet = write_index + offset;
        /*if(current_OffSet == 405504){
            printf("write buffer for 405504");
            fsp_print_buffer_hexa(wr_buf, 4096);
        }*/
        //char print_block[4097] = {0};
        //memcpy(print_block, wr_buf, block_size);
        //printf("BUFFER: %s\n", buffer);  
    
        // write given data to underlying device
        
        int status = fbd_check_and_inject_write_fault(device, wr_buf, block_size, current_OffSet);
        if(status == FBD_STS_MEDIUM_ERROR) {
            printf("Returning ENOMEDIUM\n");
            return ENOMEDIUM;
        } 
        write_index += block_size;
        wr_buf += block_size;
    }

    while (size > 0){
        ssize_t res = pwrite(fd, buffer, (size_t)size, (off_t)offset);
        if (res < 0){
            // write failed, retry if interrupted, fail otherwise

            if (errno != EINTR)
                return errno; // not interrupted, return errno
        }
        else if (res == 0){
            // should not happen
            return EIO;
        }
        else{
            // successfully wrote some data
            buffer += res;
            offset += (uint64_t)res;    wr_buf = (char *) buffer;

            size   -= (uint32_t)res;
        }
    }

    // success
    return 0;
}

static int device_write_zeros(
    uint64_t offset, uint32_t size, bool may_unmap,
    struct bdus_dev *dev
    )
{

    FBD_Device device = (FBD_Device) dev->user_data;
    int fd = device->fd;

    // issue ioctl to write zeros to underlying device

    uint64_t range[2] = { offset, (uint64_t)size };

    if (ioctl(fd, BLKZEROOUT, range) != 0)
        return errno; // failed, return errno

    // success

    return 0;
}

static int device_flush(struct bdus_dev *dev)
{
    FBD_Device device = (FBD_Device) dev->user_data;
    int fd = device->fd;

    // flush entire underlying device

    if (fdatasync(fd) != 0)
        return errno; // failed, return errno

    // success

    return 0;
}

static int device_discard(
    uint64_t offset, uint32_t size,
    struct bdus_dev *dev
    )
{
    FBD_Device device = (FBD_Device) dev->user_data;
    int fd = device->fd;

    // issue ioctl to discard data from underlying device

    uint64_t range[2] = { offset, (uint64_t)size };

    if (ioctl(fd, BLKDISCARD, range) != 0)
        return errno; // failed, return errno

    // success

    return 0;
}

static int device_secure_erase(
    uint64_t offset, uint32_t size,
    struct bdus_dev *dev
    )
{
    FBD_Device device = (FBD_Device) dev->user_data;
    int fd = device->fd;


    // issue ioctl to securely erase data from underlying device

    uint64_t range[2] = { offset, (uint64_t)size };

    if (ioctl(fd, BLKSECDISCARD, range) != 0)
        return errno; // failed, return errno

    // success

    return 0;
}

static int device_ioctl(
    uint32_t command, void *argument,
    struct bdus_dev *dev
    )
{
    FBD_Device device = (FBD_Device) dev->user_data;
    int fd = device->fd;

    // issue same ioctl to underlying device

    int result = ioctl(fd, (unsigned long)command, argument);

    if (result == -1)
        return errno; // error, return errno

    // success

    return 0;
}

static const struct bdus_ops device_ops =
{
    // (the discard and secure_erase operations are filled in by
    // configure_device())

    .read           = device_read,
    .write          = device_write,
    .write_zeros    = device_write_zeros,

    .flush          = device_flush,

    .ioctl          = device_ioctl,
    .initialize     = device_initalize,
};

static struct bdus_attrs device_attrs =
{
    // (the size, logical_block_size, and physical_block_size attributes are
    // filled in by configure_device())

    .max_concurrent_callbacks = 1, // no pararrel requests, 
    .dont_daemonize = false
};

static struct bdus_attrs get_device_attrs(bool dont_deomon){
    device_attrs.dont_daemonize = dont_deomon;
    return device_attrs;
}

/* -------------------------------------------------------------------------- */

// opens the underlying device
static int open_underlying_device(const char *file_path)
{
    // use O_DIRECT to avoid double caching

    int fd = open(file_path, O_RDWR | O_DIRECT);

    if (fd < 0)
    {
        fprintf(
            stderr, "Error: Failed to open underlying device (%s).\n",
            strerror(errno)
            );
    }

    return fd;
}

static bool configure_device_discard(int fd, struct bdus_ops *ops)
{
    // submit empty discard request and inspect resulting error to determine if
    // device supports it

    uint64_t range[2] = { 0, 0 };

    if (ioctl(fd, BLKDISCARD, range) == 0 || errno == EINVAL)
    {
        // device supports discard
        ops->discard = device_discard;
        return true;
    }
    else if (errno == EOPNOTSUPP)
    {
        // device does not support discard
        ops->discard = NULL;
        return true;
    }
    else
    {
        // some unexpected error occurred

        return false;
    }
}

static bool configure_device_secure_erase(int fd, struct bdus_ops *ops)
{
    // submit empty secure erase request and inspect resulting error to
    // determine if device supports it

    uint64_t range[2] = { 0, 0 };

    if (ioctl(fd, BLKSECDISCARD, range) == 0 || errno == EINVAL)
    {
        // device supports secure erase
        ops->secure_erase = device_secure_erase;
        return true;
    }
    else if (errno == EOPNOTSUPP)
    {
        // device does not support secure erase
        ops->secure_erase = NULL;
        return true;
    }
    else
    {
        // some unexpected error occurred
        return false;
    }
}

// sets the size, logical block size, and physical block size attributes to
// match those of the given file descriptor, which must refer to a block device
static bool configure_device(
    int fd,
    struct bdus_ops *ops,
    struct bdus_attrs *attrs
    )
{
    // support discard / secure erase only if underlying device does

    if (!configure_device_discard(fd, ops))
        return false;

    if (!configure_device_secure_erase(fd, ops))
        return false;

    // mirror the size, logical block size, and physical block size of the
    // underlying device

    if (ioctl(fd, BLKGETSIZE64, &attrs->size) != 0)
        return false;

    if (ioctl(fd, BLKSSZGET, &attrs->logical_block_size) != 0)
        return false;

    if (ioctl(fd, BLKPBSZGET, &attrs->physical_block_size) != 0)
        return false;

    // success

    return true;
}

/* -------------------------------------------------------------------------- */

static void print_usage(const char *program_name)
{
    fprintf(
        stderr, "Usage: %s <block_device> [<existing_dev_path_or_index>]\n",
        program_name
        );
}

void fbd_device_set_hash_type(FBD_Device dev, char* arg, bool* invalid_opts){
    if(strcmp(optarg, "MD5") == 0){
        device->hash_type = FBD_HASH_MD5;
    } else if(strcmp(optarg, "XXH3_128") == 0){
        device->hash_type = FBD_HASH_XXH3_128;
        device->hash_gen.xxh3_128 = XXH3_createState();
    } else if(strcmp(optarg, "MURMUR_x86_128")){ 
        device->hash_type = FBD_HASH_MURMUR_x86_128;
    } else {
        *invalid_opts = true;
    }
}

int main(int argc, char **argv){
    int option;
    char *underlying_device = NULL;
    device = fbd_new_device(-1);
    // configure device from metadata about underlying device
    bool dont_daemon = true;
    bool invalid_opts = false;

    while((option = getopt(argc, argv, "u:bh:d:DP")) != -1){
        switch (option){
            case 'u':
                underlying_device = strdup(optarg);
                break;
            case 'b':
                device->user_settings->block_mode = true;
                break;
            case 'h':
                device->user_settings->hash_mode = true;
                fbd_device_set_hash_type(device, optarg, &invalid_opts);
                break;
            case 'd':
                device->user_settings->dedup_mode = true;
                fbd_device_set_hash_type(device, optarg, &invalid_opts);
                break;
            case 'D':
                device->user_settings->device_mode = true;
                break;
            case 'P':
                dont_daemon = false;
                break;
            case '?':
                if(optopt == 'u'){
                    fprintf(stderr, "Missing argument for option '-%c'\n", optopt);
                } else {
                    fprintf(stderr, "Unknown caracther '-%c'\n", optopt);
                }
                return 2;
            default:
                fprintf(stderr, "Unknow option '-%c'\n", optopt);
                return 2;
        }
    }

    if(invalid_opts){
        printf("Invalid options\n");
        exit(0);
    }

    struct bdus_ops ops = device_ops;
    struct bdus_attrs attrs = get_device_attrs(dont_daemon);

    if(device->user_settings->dedup_mode && device->user_settings->hash_mode){
        fprintf(stderr, "Dedup and Hash mode can't be used simultaniously. Choose Hashmode if the upper\
            upper system is a file system, Dedup if it is a deduplication system\n");
        return 3;
    }

    if (underlying_device==NULL) {
        print_usage(argv[0]);
        return 2;
    }

    // open underlying device

    int fd = open_underlying_device(underlying_device);

    if (fd < 0)
        return 1;

    if (!configure_device(fd, &ops, &attrs))
    {
        fprintf(
            stderr,
            "Error: ioctl on underlying device failed. Is \"%s\" a block"
            " special file?\n",
            argv[1]
            );

        close(fd); // close underlying device
        return 1;
    }

    // run driver

    bool success;
    pthread_t server_thread_id;
    device->fd = fd;
    fbd_print_user_settings(device);	
    if (underlying_device != NULL){
        // create new device and run driver
        //Shared memory from server and bdus in "device"
        fsp_startServer_Thread(device, &server_thread_id);
        success = bdus_run(&ops, &attrs, device);
    }

    // close underlying device

    close(fd);
    free(underlying_device);
    
    // print error message if driver failed

    if (!success){
        pthread_cancel(server_thread_id);
        fprintf(stderr, "Error: %s\n", bdus_get_error_message());
    }

    // exit with appropriate exit code

    return success ? 0 : 1;
}
/* -------------------------------------------------------------------------- */
