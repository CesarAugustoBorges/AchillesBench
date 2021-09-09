/* -------------------------------------------------------------------------- */

// The driver featured in the Quick Start Guide, implementing a 1 GiB RAM-based
// volatile device. See ram.c for a slightly more involved RAM disk.

// Compile with:
//     cc ram-simple.c -lbdus -o ram-simple

/* -------------------------------------------------------------------------- */

#include <bdus.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------- */

static int device_read(
    char *buffer, uint64_t offset, uint32_t size,
    struct bdus_dev *dev
    )
{
    memcpy(buffer, (char *)dev->user_data + offset, size);
    return 0;
}

static int device_write(
    const char *buffer, uint64_t offset, uint32_t size,
    struct bdus_dev *dev
    )
{
    memcpy((char *)dev->user_data + offset, buffer, size);
    return 0;
}

static const struct bdus_ops device_ops =
{
    .read  = device_read,
    .write = device_write,
};

static const struct bdus_attrs device_attrs =
{
    .size                = 1 << 30, // 1 GiB
    .logical_block_size  = 512,
};

/* -------------------------------------------------------------------------- */

int main(void)
{
    void *buffer = malloc(device_attrs.size);

    if (!buffer)
        return 1;

    bool success = bdus_run(&device_ops, &device_attrs, buffer);

    free(buffer);

    return success ? 0 : 1;
}

/* -------------------------------------------------------------------------- */
