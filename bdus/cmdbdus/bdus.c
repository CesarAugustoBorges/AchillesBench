/* -------------------------------------------------------------------------- */
/* includes */

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <bdus.h>

/* -------------------------------------------------------------------------- */
/* messages */

// no command

static const char *const usage =
"Usage: bdus [<options...>] <command> <args...>\n"
"Try `bdus --help` for more information.\n";

static const char *const help =
"USAGE\n"
"   bdus [<options...>] <command> <args...>\n"
"\n"
"DESCRIPTION\n"
"   Manage devices created using BDUS, a framework for developing Block\n"
"   Devices in User Space (https://github.com/albertofaria/bdus).\n"
"\n"
"   Try `bdus <command> --help` for more information on a specific command.\n"
"\n"
"COMMANDS\n"
"   destroy   Destroy a device.\n"
"\n"
"OPTIONS\n"
"   --help      Print this help message.\n"
"   --version   Print the versions of this command, libbdus, and kbdus.\n";

// command "destroy"

static const char *const usage_destroy =
"Usage: bdus destroy [<options...>] <dev_path_or_index>\n"
"Try `bdus destroy --help` for more information.\n";

static const char *const help_destroy =
"USAGE\n"
"   bdus destroy [<options...>] <dev_path_or_index>\n"
"\n"
"DESCRIPTION\n"
"   Destroy a device, ensuring that data previously written to it is\n"
"   persistently stored beforehand.\n"
"\n"
"ARGUMENTS\n"
"   <dev_path_or_index>   The path or index of the device to be destroyed.\n"
"\n"
"OPTIONS\n"
"   --help       Print this help message.\n"
"   --no-flush   Skip flushing of previously written data.\n";

/* -------------------------------------------------------------------------- */
/* actions */

static int action_version(void)
{
    // cmdbdus version

    fprintf(
        stdout,
        "cmdbdus %"PRIu32".%"PRIu32".%"PRIu32"\n",
        (uint32_t)BDUS_VERSION_MAJOR,
        (uint32_t)BDUS_VERSION_MINOR,
        (uint32_t)BDUS_VERSION_PATCH
        );

    // libbdus version

    const struct bdus_version *const libbdus_ver = bdus_get_libbdus_version();

    fprintf(
        stdout,
        "libbdus %"PRIu32".%"PRIu32".%"PRIu32"\n",
        libbdus_ver->major, libbdus_ver->minor, libbdus_ver->patch
        );

    // kbdus version

    struct bdus_version kbdus_ver;

    if (!bdus_get_kbdus_version(&kbdus_ver))
    {
        fflush(stdout); // ensure versions appear before error

        fprintf(
            stderr,
            "Error: Failed to get kbdus version: %s\n",
            bdus_get_error_message()
            );

        return 1;
    }

    fprintf(
        stdout,
        "kbdus %"PRIu32".%"PRIu32".%"PRIu32"\n",
        kbdus_ver.major, kbdus_ver.minor, kbdus_ver.patch
        );

    // success

    return 0;
}

static int action_destroy(const char *dev_path_or_index, bool flush_dev)
{
    // get device index

    uint32_t dev_index;

    if (!bdus_dev_index_or_path_to_index(&dev_index, dev_path_or_index))
    {
        fprintf(
            stderr, "Error: '%s' is not a valid device path or index.\n",
            dev_path_or_index
            );

        return 1;
    }

    // flush device

    if (!bdus_destroy_dev(dev_index, flush_dev))
    {
        fprintf(stderr, "Error: %s\n", bdus_get_error_message());
        return 1;
    }

    // success

    return 0;
}

/* -------------------------------------------------------------------------- */
/* main */

int main(int argc, char **argv)
{
    // parse arguments and run corresponding actions

    if (argc == 2 && strcmp(argv[1], "--help") == 0)
    {
        fputs(help, stdout);
        return 0;
    }
    else if (argc == 2 && strcmp(argv[1], "--version") == 0)
    {
        return action_version();
    }
    else if (argc >= 2 && strcmp(argv[1], "destroy") == 0)
    {
        if (argc == 3 && strcmp(argv[2], "--help") == 0)
        {
            fputs(help_destroy, stdout);
            return 0;
        }
        else if (argc == 3)
        {
            return action_destroy(argv[2], true);
        }
        else if (argc == 4 && strcmp(argv[2], "--no-flush") == 0)
        {
            return action_destroy(argv[3], false);
        }
        else if (argc == 4 && strcmp(argv[3], "--no-flush") == 0)
        {
            return action_destroy(argv[2], false);
        }
        else
        {
            fputs(usage_destroy, stderr);
            return 2;
        }
    }
    else
    {
        fputs(usage, stderr);
        return 2;
    }
}

/* -------------------------------------------------------------------------- */
