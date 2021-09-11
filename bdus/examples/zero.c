/* -------------------------------------------------------------------------- */

// A read-only, zero-filled 1 GiB device.

// This driver can both create a new device or attach to an existing device
// (replacing that device's current driver).

// Compile with:
//     cc zero-replace.c -lbdus -o zero-replace

/* -------------------------------------------------------------------------- */

#include <bdus.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------- */

static int device_read(
    char *buffer, uint64_t offset, uint32_t size,
    struct bdus_dev *dev
    )
{
    memset(buffer, 0, (size_t)size); // zero-fill request buffer
    return 0; // success
}

static const struct bdus_ops device_ops =
{
    .read = device_read,
};

static const struct bdus_attrs device_attrs =
{
    .size                     = 1ull << 30, // 1 GiB
    .logical_block_size       = 512,

    .max_concurrent_callbacks = 16, // enable parallel request processing
};

/* -------------------------------------------------------------------------- */

static void print_usage(const char *program_name)
{
    fprintf(stderr, "Usage: %s [<existing_dev_path_or_index>]\n", program_name);
}

int main(int argc, char **argv)
{
    bool success;

    if (argc == 1)
    {
        // create device and run driver

        success = bdus_run(&device_ops, &device_attrs, NULL);
    }
    else if (argc == 2)
    {
        // parse path or index of existing device

        uint32_t dev_index;

        if (!bdus_dev_index_or_path_to_index(&dev_index, argv[1]))
        {
            print_usage(argv[0]);
            return 2;
        }

        // run driver for existing device

        success = bdus_rerun(dev_index, &device_ops, &device_attrs, NULL);
    }
    else
    {
        print_usage(argv[0]);
        return 2;
    }

    // print error message if driver failed

    if (!success)
        fprintf(stderr, "Error: %s\n", bdus_get_error_message());

    // exit with appropriate exit code

    return success ? 0 : 1;
}

/* -------------------------------------------------------------------------- */
