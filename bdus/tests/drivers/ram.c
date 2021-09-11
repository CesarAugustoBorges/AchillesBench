/* -------------------------------------------------------------------------- */

#define _XOPEN_SOURCE 500

#include <bdus.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* -------------------------------------------------------------------------- */

#define BDUS_TEST_MAX_DELAY_US 2000

static int device_initialize(struct bdus_dev *dev)
{
    dev->user_data = malloc((size_t)dev->attrs->size);
    return dev->user_data ? 0 : ENOMEM;
}

static int device_terminate(struct bdus_dev *dev)
{
    free(dev->user_data);
    return 0;
}

static int device_read(
    char *buffer, uint64_t offset, uint32_t size,
    struct bdus_dev *dev
    )
{
    memcpy(buffer, (char *)dev->user_data + offset, (size_t)size);

    usleep(rand() * BDUS_TEST_MAX_DELAY_US / RAND_MAX);

    return 0;
}

static int device_write(
    const char *buffer, uint64_t offset, uint32_t size,
    struct bdus_dev *dev
    )
{
    memcpy((char *)dev->user_data + offset, buffer, (size_t)size);
    __sync_synchronize();

    usleep(rand() * BDUS_TEST_MAX_DELAY_US / RAND_MAX);

    return 0;
}

static int device_write_same(
    const char *buffer, uint64_t offset, uint32_t size,
    struct bdus_dev *dev
    )
{
    uint32_t lbs = dev->attrs->logical_block_size;

    for (uint64_t i = offset; i < offset + size; i += (uint64_t)lbs)
        memcpy((char *)dev->user_data + i, buffer, (size_t)lbs);

    __sync_synchronize();

    usleep(rand() * BDUS_TEST_MAX_DELAY_US / RAND_MAX);

    return 0;
}

static int device_write_zeros(
    uint64_t offset, uint32_t size, bool may_unmap,
    struct bdus_dev *dev
    )
{
    memset((char *)dev->user_data + offset, 0, (size_t)size);
    __sync_synchronize();

    usleep(rand() * BDUS_TEST_MAX_DELAY_US / RAND_MAX);

    return 0;
}

static int device_flush(struct bdus_dev *dev)
{
    usleep(rand() * BDUS_TEST_MAX_DELAY_US / RAND_MAX);

    return 0;
}

static int device_discard(
    uint64_t offset, uint32_t size,
    struct bdus_dev *dev
    )
{
    usleep(rand() * BDUS_TEST_MAX_DELAY_US / RAND_MAX);

    return 0;
}

static int device_secure_erase(
    uint64_t offset, uint32_t size,
    struct bdus_dev *dev
    )
{
    usleep(rand() * BDUS_TEST_MAX_DELAY_US / RAND_MAX);

    return 0;
}

static int device_ioctl(
    uint32_t command, void *argument,
    struct bdus_dev *dev
    )
{
    usleep(rand() * BDUS_TEST_MAX_DELAY_US / RAND_MAX);

    return ENOTTY;
}

static const struct bdus_ops device_ops =
{
    .initialize   = device_initialize,
    .terminate    = device_terminate,

    .read         = device_read,
    .write        = device_write,
    .write_same   = device_write_same,
    .write_zeros  = device_write_zeros,
    .flush        = device_flush,
    .discard      = device_discard,
    .secure_erase = device_secure_erase,
    .ioctl        = device_ioctl,
};

static const struct bdus_attrs device_attrs =
{
    .size                     = 1 << 30, // 1 GiB
    .logical_block_size       = 512,

    .max_concurrent_callbacks = 8,
};

static const struct bdus_internal_config_ device_internal_config =
{
    .max_active_queue_reqs = 16,
    .max_active_ioctl_reqs =  2,
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

    bool success = bdus_run_with_internal_config_(
        &device_ops, &device_attrs, NULL, &device_internal_config
        );

    // print error message if driver failed

    if (!success)
        fprintf(stderr, "Error: %s\n", bdus_get_error_message());

    // exit with appropriate exit code

    return success ? 0 : 1;
}

/* -------------------------------------------------------------------------- */
