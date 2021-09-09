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

/* -------------------------------------------------------------------------- */

static int device_read(
    char *buffer, uint64_t offset, uint32_t size,
    struct bdus_dev *dev
    )
{
    int fd = *(int *)dev->user_data;

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

    // success

    return 0;
}

static int device_write(
    const char *buffer, uint64_t offset, uint32_t size,
    struct bdus_dev *dev
    )
{
    int fd = *(int *)dev->user_data;

    // write given data to underlying device

    while (size > 0)
    {
        ssize_t res = pwrite(fd, buffer, (size_t)size, (off_t)offset);

        if (res < 0)
        {
            // write failed, retry if interrupted, fail otherwise

            if (errno != EINTR)
                return errno; // not interrupted, return errno
        }
        else if (res == 0)
        {
            // should not happen

            return EIO;
        }
        else
        {
            // successfully wrote some data

            buffer += res;
            offset += (uint64_t)res;
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
    int fd = *(int *)dev->user_data;

    // issue ioctl to write zeros to underlying device

    uint64_t range[2] = { offset, (uint64_t)size };

    if (ioctl(fd, BLKZEROOUT, range) != 0)
        return errno; // failed, return errno

    // success

    return 0;
}

static int device_flush(struct bdus_dev *dev)
{
    int fd = *(int *)dev->user_data;

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
    int fd = *(int *)dev->user_data;

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
    int fd = *(int *)dev->user_data;

    // issue ioctl to securely discard data from underlying device

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
    int fd = *(int *)dev->user_data;

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
};

static const struct bdus_attrs device_attrs =
{
    // (the size, logical_block_size, and physical_block_size attributes are
    // filled in by configure_device())

    .max_concurrent_callbacks = 32,

    .recoverable = true,
};

static const struct bdus_internal_config_ device_internal_config =
{
    .backend_name          = "",

    .max_active_queue_reqs = 64,
    .max_active_ioctl_reqs =  2,
};

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

static bool configure_device_discard(int fd, bool secure, struct bdus_ops *ops)
{
    // submit empty discard / secure discard request and inspect resulting error
    // to find out if device supports it

    unsigned long command = secure ? BLKSECDISCARD : BLKDISCARD;
    uint64_t range[2] = { 0, 0 };

    if (ioctl(fd, command, range) == 0 || errno == EINVAL)
    {
        // device supports discard / secure discard

        if (secure)
            ops->secure_erase = device_secure_erase;
        else
            ops->discard = device_discard;

        // success

        return true;
    }
    else if (errno == EOPNOTSUPP)
    {
        // device does not support discard / secure discard

        if (secure)
            ops->secure_erase = NULL;
        else
            ops->discard = NULL;

        // success

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
    struct stat statbuf;

    if (fstat(fd, &statbuf) != 0)
        return false;

    if (!S_ISBLK(statbuf.st_mode))
    {
        fprintf(
            stderr, "Error: Underlying file must be a block special file.\n"
            );

        return false;
    }

    // support discard / secure discard only if underlying device does

    if (!configure_device_discard(fd, false, ops))
        return false;

    if (!configure_device_discard(fd, true, ops))
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
        stderr, "Usage: %s <backend> <device_or_file> [<dev_path_or_index>]\n",
        program_name
        );
}

int main(int argc, char **argv)
{
    // validate usage

    if (argc != 3 && argc != 4)
    {
        print_usage(argv[0]);
        return 2;
    }

    // open underlying device

    int fd = open_underlying_device(argv[2]);

    if (fd < 0)
        return 1;

    // configure device from metadata about underlying device

    struct bdus_ops ops = device_ops;
    struct bdus_attrs attrs = device_attrs;

    if (!configure_device(fd, &ops, &attrs))
    {
        close(fd); // close underlying device
        return 1;
    }

    // run driver

    struct bdus_internal_config_ internal_config = device_internal_config;
    internal_config.backend_name = argv[1];

    bool success;

    if (argc == 3)
    {
        // create new device and run driver

        success = bdus_run_with_internal_config_(
            &ops, &attrs, &fd, &internal_config
            );
    }
    else // argc == 4
    {
        // parse path or index of existing device

        uint32_t dev_index;

        if (!bdus_dev_index_or_path_to_index(&dev_index, argv[3]))
        {
            close(fd); // close underlying device
            print_usage(argv[0]);
            return 2;
        }

        // attach to existing device and run driver

        success = bdus_rerun_with_internal_config_(
            dev_index, &ops, &attrs, &fd, &internal_config
            );
    }

    // close underlying device

    close(fd);

    // print error message if driver failed

    if (!success)
        fprintf(stderr, "Error: %s\n", bdus_get_error_message());

    // exit with appropriate exit code

    return success ? 0 : 1;
}

/* -------------------------------------------------------------------------- */
