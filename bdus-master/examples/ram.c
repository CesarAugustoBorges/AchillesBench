/* -------------------------------------------------------------------------- */

// A 1 GiB RAM-based volatile device.

// Compile with:
//     cc ram.c -lbdus -o ram

/* -------------------------------------------------------------------------- */

#include <bdus.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------- */

static int device_initialize(struct bdus_dev *dev)
{
    // allocate internal buffer to store device contents
    // (could also do this in main() and pass the buffer to bdus_run())

    dev->user_data = malloc((size_t)dev->attrs->size);

    if (!dev->user_data)
        return ENOMEM; // allocation failed

    return 0; // success
}

static int device_terminate(struct bdus_dev *dev)
{
    // free internal buffer

    free(dev->user_data);

    return 0; // success
}

static int device_read(
    char *buffer, uint64_t offset, uint32_t size,
    struct bdus_dev *dev
    )
{
    // copy data from internal buffer to request buffer

    memcpy(buffer, (char *)dev->user_data + offset, (size_t)size);

    return 0; // success
}

static int device_write(
    const char *buffer, uint64_t offset, uint32_t size,
    struct bdus_dev *dev
    )
{
    // copy data from request buffer to internal buffer

    memcpy((char *)dev->user_data + offset, buffer, (size_t)size);

    __sync_synchronize(); // ensure other threads immediately see written data

    return 0; // success
}

static int device_write_same(
    const char *buffer, uint64_t offset, uint32_t size,
    struct bdus_dev *dev
    )
{
    // copy data from request buffer to internal buffer several times

    uint32_t lbs = dev->attrs->logical_block_size;

    for (uint64_t i = offset; i < offset + size; i += (uint64_t)lbs)
        memcpy((char *)dev->user_data + i, buffer, (size_t)lbs);

    __sync_synchronize(); // ensure other threads immediately see written data

    return 0; // success
}

static int device_write_zeros(
    uint64_t offset, uint32_t size, bool may_unmap,
    struct bdus_dev *dev
    )
{
    // set range of internal buffer to zeros

    memset((char *)dev->user_data + offset, 0, (size_t)size);

    __sync_synchronize(); // ensure other threads immediately see written data

    return 0; // success
}

static const struct bdus_ops device_ops =
{
    .initialize  = device_initialize,
    .terminate   = device_terminate,

    .read        = device_read,
    .write       = device_write,
    .write_same  = device_write_same,
    .write_zeros = device_write_zeros,
};

static const struct bdus_attrs device_attrs =
{
    .size                     = 1ull << 30, // 1 GiB
    .logical_block_size       = 512,

    .max_concurrent_callbacks = 16, // enable parallel request processing
};

/* -------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    // validate usage

    if (argc != 1)
    {
        fprintf(stderr, "Usage: %s\n", argv[0]);
        return 2;
    }

    // create device and run driver

    bool success = bdus_run(&device_ops, &device_attrs, NULL);

    // print error message if driver failed

    if (!success)
        fprintf(stderr, "Error: %s\n", bdus_get_error_message());

    // exit with appropriate exit code

    return success ? 0 : 1;
}

/* -------------------------------------------------------------------------- */
