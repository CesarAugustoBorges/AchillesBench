/* -------------------------------------------------------------------------- */
/* includes */

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200112L
#define _XOPEN_SOURCE 500

#include <bdus.h>
#include <libbdus/backend.h>
#include <libbdus/utilities.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <kbdus.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* -------------------------------------------------------------------------- */
/* private utilities */

static bool bdus_check_privileges_(void)
{
    // TODO: This is here only to give a better error message if an user tries
    // to run a driver or manage a device without having sufficient privileges
    // (if we didn't do this and kbdus was unloaded, we would silently fail to
    // load it and the subsequent open of /dev/bdus-control would just say that
    // it doesn't exist, instead of saying that the caller doesn't have enough
    // permissions), but the check for root is not entirely correct. Ideally,
    // one would check the process capabilities, but that introduces a
    // dependency on libcap, which may require a package installation.

    if (geteuid() == (uid_t)0)
    {
        return true;
    }
    else
    {
        bdus_set_error_dont_append_errno_(
            EPERM,
            "Insufficient privileges, must be run as the root user."
            );

        return false;
    }
}

static void bdus_try_to_load_kbdus_(void)
{
    const int previous_errno = errno;

    const pid_t fork_result = fork();

    if (fork_result < 0)
    {
        // error
    }
    else if (fork_result > 0)
    {
        // in parent

        waitpid(fork_result, NULL, 0);
    }
    else
    {
        // in child

        // redirect stdin, stdout, and stderr to /dev/null

        if (!bdus_redirect_to_dev_null_(STDIN_FILENO, O_RDONLY))
            _exit(1);

        if (!bdus_redirect_to_dev_null_(STDOUT_FILENO, O_WRONLY))
            _exit(1);

        if (!bdus_redirect_to_dev_null_(STDERR_FILENO, O_WRONLY))
            _exit(1);

        // exec modprobe

        const char modprobe_path[] = "/sbin/modprobe";
        const char *const modprobe_env[] = { NULL };

        execle(
            modprobe_path,
            modprobe_path, "kbdus", (char *)NULL,
            modprobe_env
            );

        // error

        _exit(1);
    }

    errno = previous_errno;
}

static bool bdus_check_version_compatibility_(int control_fd)
{
    // get kbdus version

    struct kbdus_version kbdus_ver;

    if (bdus_ioctl_arg_retry_(
        control_fd, KBDUS_IOCTL_GET_VERSION, &kbdus_ver
        ) == -1)
    {
        bdus_set_error_(
            errno,
            "Failed to issue ioctl with command KBDUS_IOCTL_GET_VERSION to"
            " /dev/bdus-control."
            );

        return false;
    }

    // check if kbdus and libbdus versions are the same

    const struct bdus_version *const libbdus_ver = bdus_get_libbdus_version();

    // TODO: use this condition when moving to 1.0.0
    // if (kbdus_ver.major != libbdus_ver->major ||
    //     kbdus_ver.minor < libbdus_ver->minor)

    // TODO: use this condition when moving to 0.1.0
    // if (kbdus_ver.major != libbdus_ver->major ||
    //     kbdus_ver.minor != libbdus_ver->minor)

    if (kbdus_ver.major != libbdus_ver->major ||
        kbdus_ver.minor != libbdus_ver->minor ||
        kbdus_ver.patch != libbdus_ver->patch)
    {
        bdus_set_error_dont_append_errno_(
            EINVAL,
            "Using libbdus version %"PRIu32".%"PRIu32".%"PRIu32" but kbdus has"
            " incompatible version %"PRIu32".%"PRIu32".%"PRIu32".",
            libbdus_ver->major, libbdus_ver->minor, libbdus_ver->patch,
            kbdus_ver.major, kbdus_ver.minor, kbdus_ver.patch
            );

        return false;
    }

    // versions are the same

    return true;
}

static int bdus_open_control_(bool check_version_compatibility)
{
    // check if driver has sufficient privileges

    if (!bdus_check_privileges_())
        return -1;

    // try to load kbdus

    bdus_try_to_load_kbdus_();

    // open control device

    const int control_fd = bdus_open_retry_("/dev/bdus-control", O_RDWR);

    if (control_fd < 0)
    {
        if (errno == ENOENT)
        {
            bdus_set_error_dont_append_errno_(
                errno,
                "/dev/bdus-control does not exist. Is the kbdus module"
                " installed and loaded? (Failed to load it automatically.)"
                );
        }
        else if (errno == EACCES || errno == EPERM)
        {
            bdus_set_error_dont_append_errno_(
                errno,
                "Access to /dev/bdus-control was denied. Is the application"
                " being run with sufficient privileges?"
                );
        }
        else
        {
            bdus_set_error_(errno, "Failed to open /dev/bdus-control.");
        }

        return -1;
    }

    // check version compatibility with kbdus

    if (check_version_compatibility)
    {
        if (!bdus_check_version_compatibility_(control_fd))
        {
            bdus_close_keep_errno_(control_fd);
            return -1;
        }
    }

    // success

    return control_fd;
}

static bool bdus_get_dev_seqnum_(
    int control_fd,
    uint32_t dev_index,
    uint64_t *out_dev_seqnum
    )
{
    // translate index to seqnum

    uint64_t dev_index64_or_seqnum = (uint64_t)dev_index;

    const int ret = bdus_ioctl_arg_retry_(
        control_fd,
        KBDUS_IOCTL_DEVICE_INDEX_TO_SEQNUM,
        &dev_index64_or_seqnum
        );

    if (ret == -1)
    {
        if (errno == ENODEV)
        {
            bdus_set_error_dont_append_errno_(
                errno,
                "Device /dev/bdus-%"PRIu32" does not exist.",
                dev_index
                );
        }
        else
        {
            bdus_set_error_(
                errno,
                "Failed to issue ioctl with command"
                " KBDUS_IOCTL_DEVICE_INDEX_TO_SEQNUM to /dev/bdus-control."
                );
        }

        return false;
    }

    // success

    *out_dev_seqnum = dev_index64_or_seqnum;

    return true;
}

/* -------------------------------------------------------------------------- */
/* driver development -- common */

static const struct bdus_backend_ *bdus_adjust_internal_config_(
    struct bdus_internal_config_ *internal_config
    )
{
    // adjust internal configuration

    if (!internal_config->backend_name)
        internal_config->backend_name = "mmap";

    if (internal_config->max_active_queue_reqs == 0)
        internal_config->max_active_queue_reqs = 64;

    if (internal_config->max_active_ioctl_reqs == 0)
        internal_config->max_active_ioctl_reqs = 16;

    // lookup backend

    const struct bdus_backend_ *const backend = bdus_backend_lookup_(
        internal_config->backend_name
        );

    if (!backend)
    {
        bdus_set_error_dont_append_errno_(
            EINVAL,
            "Unknown backend '%s'.", internal_config->backend_name
            );
    }

    // return backend

    return backend;
}

static bool bdus_execute_driver_(
    const struct bdus_ops *ops_copy,
    const struct bdus_attrs *attrs_copy,
    void *user_data,
    int control_fd,
    const struct kbdus_device_config *device_config,
    const struct bdus_internal_config_ *adjusted_internal_config,
    const struct bdus_backend_ *backend,
    bool is_rerun
    )
{
    // get device path

    char dev_path[32];

    if (bdus_dev_index_to_path(
        dev_path, sizeof(dev_path), device_config->index
        ) == 0)
    {
        return false;
    }

    // create bdus_dev structure

    struct bdus_dev dev =
    {
        .index       = device_config->index,
        .path        = dev_path,

        .major       = device_config->major,
        .first_minor = device_config->first_minor,

        .ops         = ops_copy,
        .attrs       = attrs_copy,

        .is_rerun    = is_rerun,

        .user_data   = user_data,
    };

    // invoke `initialize()` callback

    if (dev.ops->initialize)
    {
        if (dev.attrs->log_callbacks)
            bdus_log_thread_no_args_(0, "initialize(dev)");

        const int ret = dev.ops->initialize(&dev);

        if (ret != 0)
        {
            bdus_set_error_(ret, "Driver callback initialize() failed.");
            return false;
        }
    }
    else
    {
        if (dev.attrs->log_callbacks)
            bdus_log_thread_no_args_(0, "initialize(dev) [not implemented]");
    }

    // delegate work to backend

    const bool success = backend->run(
        control_fd, device_config->seqnum, &dev, adjusted_internal_config
        );

    // invoke `terminate()` callback

    if (dev.ops->terminate)
    {
        if (dev.attrs->log_callbacks)
            bdus_log_thread_no_args_(0, "terminate(dev)");

        const int e = errno;
        const int ret = dev.ops->terminate(&dev);
        errno = e;

        if (success && ret != 0) // avoid replacing previous error
        {
            bdus_set_error_(ret, "Driver callback terminate() failed.");
            return false;
        }
    }
    else
    {
        if (dev.attrs->log_callbacks)
            bdus_log_thread_no_args_(0, "terminate(dev) [not implemented]");
    }

    // report success to kbdus

    if (success)
    {
        if (bdus_ioctl_retry_(control_fd, KBDUS_IOCTL_MARK_AS_SUCCESSFUL) != 0)
        {
            bdus_set_error_(
                errno,
                "Failed to issue ioctl with command"
                " KBDUS_IOCTL_MARK_AS_SUCCESSFUL to /dev/bdus-control."
                );

            return false;
        }
    }

    // return

    return success;
}

/* -------------------------------------------------------------------------- */
/* driver development -- bdus_run() */

static bool bdus_validate_ops_run_(const struct bdus_ops *ops)
{
    // 'fua_write' implies 'flush'

    if (ops->fua_write && !ops->flush)
    {
        bdus_set_error_dont_append_errno_(
            EINVAL,
            "The driver implements callback 'fua_write' but not 'flush'."
            );

        return false;
    }

    // success

    return true;
}

static bool bdus_validate_attrs_run_(const struct bdus_attrs *attrs)
{
    // get system page size

    const size_t page_size = bdus_get_page_size_();

    if (page_size == 0)
        return false;

    // validate 'logical_block_size'

    if (!bdus_is_power_of_two_(attrs->logical_block_size) ||
        attrs->logical_block_size < UINT32_C(512) ||
        attrs->logical_block_size > (uint32_t)page_size)
    {
       bdus_set_error_dont_append_errno_(
            EINVAL,
            "Invalid value %"PRIu32" for attribute 'logical_block_size', must"
            " be a power of two greater than or equal to 512 and less than or"
            " equal to the system's page size (which is %zu).",
            attrs->logical_block_size,
            page_size
            );

        return false;
    }

    // validate 'physical_block_size'

    if (attrs->physical_block_size != 0 && (
            !bdus_is_power_of_two_(attrs->physical_block_size) ||
            attrs->physical_block_size < attrs->logical_block_size ||
            attrs->physical_block_size > (uint32_t)page_size
            )
        )
    {
        bdus_set_error_dont_append_errno_(
            EINVAL,
            "Invalid value %"PRIu32" for attribute 'physical_block_size', must"
            " be 0 or a power of two greater than or equal to attribute"
            " 'logical_block_size' (which is %"PRIu32") and less than or equal"
            " to the system's page size (which is %zu).",
            attrs->physical_block_size,
            attrs->logical_block_size,
            page_size
            );

        return false;
    }

    // validate 'size'

    const uint32_t adjusted_physical_block_size = bdus_max_(
        attrs->physical_block_size,
        attrs->logical_block_size
        );

    if (!bdus_is_positive_multiple_of_(
        attrs->size,
        (uint64_t)adjusted_physical_block_size
        ))
    {
        bdus_set_error_dont_append_errno_(
            EINVAL,
            "Invalid value %"PRIu64" for attribute 'size', must be a positive"
            " multiple of attribute 'physical_block_size' (which is"
            " %"PRIu32").",
            attrs->size,
            adjusted_physical_block_size
            );

        return false;
    }

    // validate 'max_read_write_size'

    if (attrs->max_read_write_size != 0 &&
        attrs->max_read_write_size < (uint32_t)page_size)
    {
        bdus_set_error_dont_append_errno_(
            EINVAL,
            "Invalid value %"PRIu32" for attribute 'max_read_write_size', must"
            " be 0 or greater than or equal to the system's page size (which is"
            " %zu).",
            attrs->max_read_write_size,
            page_size
            );

        return false;
    }

    // validate 'max_write_same_size'

    if (attrs->max_write_same_size != 0 &&
        attrs->max_write_same_size < attrs->logical_block_size)
    {
        bdus_set_error_dont_append_errno_(
            EINVAL,
            "Invalid value %"PRIu32" for attribute 'max_write_same_size', must"
            " be 0 or greater than or equal to attribute 'logical_block_size'"
            " (which is %"PRIu32").",
            attrs->max_write_same_size,
            attrs->logical_block_size
            );

        return false;
    }

    // validate 'max_write_zeros_size'

    if (attrs->max_write_zeros_size != 0 &&
        attrs->max_write_zeros_size < attrs->logical_block_size)
    {
        bdus_set_error_dont_append_errno_(
            EINVAL,
            "Invalid value %"PRIu32" for attribute 'max_write_zeros_size', must"
            " be 0 or greater than or equal to attribute 'logical_block_size'"
            " (which is %"PRIu32").",
            attrs->max_write_zeros_size,
            attrs->logical_block_size
            );

        return false;
    }

    // validate 'max_discard_erase_size'

    if (attrs->max_discard_erase_size != 0 &&
        attrs->max_discard_erase_size < attrs->logical_block_size)
    {
        bdus_set_error_dont_append_errno_(
            EINVAL,
            "Invalid value %"PRIu32" for attribute 'max_discard_erase_size',"
            " must be 0 or greater than or equal to attribute"
            " 'logical_block_size' (which is %"PRIu32").",
            attrs->max_discard_erase_size,
            attrs->logical_block_size
            );

        return false;
    }

    // success

    return true;
}

static bool bdus_run_impl_(
    const struct bdus_ops *ops_copy,
    struct bdus_attrs *attrs_copy,
    void *user_data,
    int control_fd,
    const struct bdus_internal_config_ *adjusted_internal_config,
    const struct bdus_backend_ *backend
    )
{
    // validate operations and attributes

    if (!bdus_validate_ops_run_(ops_copy))
        return false;

    if (!bdus_validate_attrs_run_(attrs_copy))
        return false;

    // create configuration

    attrs_copy->max_concurrent_callbacks = bdus_min_(
        bdus_max_(attrs_copy->max_concurrent_callbacks, UINT32_C(1)),
        adjusted_internal_config->max_active_queue_reqs
            + adjusted_internal_config->max_active_ioctl_reqs
        );

    struct kbdus_config kbdus_config =
    {
        .device =
        {
            .logical_block_size     = attrs_copy->logical_block_size,
            .physical_block_size    = attrs_copy->physical_block_size,
            .size                   = attrs_copy->size,

            .max_read_write_size    = attrs_copy->max_read_write_size,
            .max_write_same_size    = attrs_copy->max_write_same_size,
            .max_write_zeros_size   = attrs_copy->max_write_zeros_size,
            .max_discard_erase_size = attrs_copy->max_discard_erase_size,

            .supports_read          = (ops_copy->read         != NULL),
            .supports_write         = (ops_copy->write        != NULL),
            .supports_write_same    = (ops_copy->write_same   != NULL),
            .supports_write_zeros   = (ops_copy->write_zeros  != NULL),
            .supports_fua_write     = (ops_copy->fua_write    != NULL),
            .supports_flush         = (ops_copy->flush        != NULL),
            .supports_discard       = (ops_copy->discard      != NULL),
            .supports_secure_erase  = (ops_copy->secure_erase != NULL),
            .supports_ioctl         = (ops_copy->ioctl        != NULL),

            .is_rotational = !adjusted_internal_config->is_not_rotational,

            .should_merge_requests =
                !adjusted_internal_config->dont_merge_requests,

            .lifo_request_transmission =
                !adjusted_internal_config->fifo_request_transmission,

            .max_active_queue_reqs =
                adjusted_internal_config->max_active_queue_reqs,

            .max_active_ioctl_reqs =
                adjusted_internal_config->max_active_ioctl_reqs,

            .disable_partition_scanning =
                attrs_copy->disable_partition_scanning,

            .destroy_when_detached = !attrs_copy->recoverable,
        },

        .protocol =
        {
            .num_request_buffers = attrs_copy->max_concurrent_callbacks,
        },
    };

    strncpy(
        kbdus_config.protocol.name,
        backend->protocol_name,
        sizeof(kbdus_config.protocol.name) - 1
        );

    // create device

    if (bdus_ioctl_arg_retry_(
        control_fd, KBDUS_IOCTL_CREATE_DEVICE, &kbdus_config
        ) != 0)
    {
        if (errno == ENOSPC)
        {
            bdus_set_error_dont_append_errno_(errno, "Too many BDUS devices.");
        }
        else
        {
            bdus_set_error_(
                errno,
                "Failed to issue ioctl with command KBDUS_IOCTL_CREATE_DEVICE"
                " to /dev/bdus-control."
                );
        }

        return false;
    }

    // adjust attributes

    attrs_copy->physical_block_size  = kbdus_config.device.physical_block_size;

    attrs_copy->max_read_write_size  = kbdus_config.device.max_read_write_size;
    attrs_copy->max_write_same_size  = kbdus_config.device.max_write_same_size;
    attrs_copy->max_write_zeros_size = kbdus_config.device.max_write_zeros_size;

    attrs_copy->max_discard_erase_size =
        kbdus_config.device.max_discard_erase_size;

    // delegate remaining work

    return bdus_execute_driver_(
        ops_copy, attrs_copy, user_data,
        control_fd, &kbdus_config.device, adjusted_internal_config, backend,
        false
        );
}

BDUS_EXPORT_ bool bdus_run(
    const struct bdus_ops *ops,
    const struct bdus_attrs *attrs,
    void *user_data
    )
{
    const struct bdus_internal_config_ internal_config = { 0 };

    return bdus_run_with_internal_config_(
        ops, attrs, user_data, &internal_config
        );
}

BDUS_EXPORT_ bool bdus_run_with_internal_config_(
    const struct bdus_ops *ops,
    const struct bdus_attrs *attrs,
    void *user_data,
    const struct bdus_internal_config_ *internal_config
    )
{
    // copy operations and attributes

    const struct bdus_ops ops_copy = *ops;
    struct bdus_attrs attrs_copy = *attrs;

    // adjust internal configuration and lookup backend

    struct bdus_internal_config_ adjusted_internal_config = *internal_config;

    const struct bdus_backend_ *const backend = bdus_adjust_internal_config_(
        &adjusted_internal_config
        );

    if (!backend)
        return false;

    // open control device

    const int control_fd = bdus_open_control_(true);

    if (control_fd < 0)
        return false;

    // delegate remaining work

    const bool success = bdus_run_impl_(
        &ops_copy, &attrs_copy, user_data,
        control_fd, &adjusted_internal_config, backend
        );

    // close control device

    bdus_close_keep_errno_(control_fd);

    // return success indication

    return success;
}

/* -------------------------------------------------------------------------- */
/* driver development -- bdus_rerun() */

static bool bdus_validate_ops_rerun_(
    const struct bdus_ops *ops,
    const struct kbdus_device_config *device_config
    )
{
    // 'fua_write' implies 'flush'

    if (ops->fua_write && !ops->flush)
    {
        bdus_set_error_dont_append_errno_(
            EINVAL,
            "The driver implements callback 'fua_write' but not 'flush'."
            );

        return false;
    }

    // check if already supported operations are still supported

    if (device_config->supports_read && !ops->read)
    {
        bdus_set_error_dont_append_errno_(
            EINVAL,
            "The device supports \"read\" requests but the driver does not"
            " implement callback 'read'."
            );
    }

    if (device_config->supports_write && !ops->write)
    {
        bdus_set_error_dont_append_errno_(
            EINVAL,
            "The device supports \"write\" requests but the driver does not"
            " implement callback 'write'."
            );
    }

    if (device_config->supports_write_same && !ops->write_same)
    {
        bdus_set_error_dont_append_errno_(
            EINVAL,
            "The device supports \"write same\" requests but the driver does"
            " not implement callback 'write_same'."
            );
    }

    if (device_config->supports_write_zeros && !ops->write_zeros)
    {
        bdus_set_error_dont_append_errno_(
            EINVAL,
            "The device supports \"write zeros\" requests but the driver does"
            " not implement callback 'write_zeros'."
            );
    }

    if (device_config->supports_fua_write && !ops->fua_write)
    {
        bdus_set_error_dont_append_errno_(
            EINVAL,
            "The device supports \"FUA write\" requests but the driver does not"
            " implement callback 'fua_write'."
            );
    }

    if (device_config->supports_flush && !ops->flush)
    {
        bdus_set_error_dont_append_errno_(
            EINVAL,
            "The device supports \"flush\" requests but the driver does not"
            " implement callback 'flush'."
            );
    }

    // (additionally, must not implement flush if device doesn't support it)
    if (!device_config->supports_flush && ops->flush)
    {
        bdus_set_error_dont_append_errno_(
            EINVAL,
            "The device does not support \"flush\" requests but the driver"
            " implements callback 'flush'."
            );
    }

    if (device_config->supports_discard && !ops->discard)
    {
        bdus_set_error_dont_append_errno_(
            EINVAL,
            "The device supports \"discard\" requests but the driver does not"
            " implement callback 'discard'."
            );
    }

    if (device_config->supports_secure_erase && !ops->secure_erase)
    {
        bdus_set_error_dont_append_errno_(
            EINVAL,
            "The device supports \"secure erase\" requests but the driver does"
            " not implement callback 'secure_erase'."
            );
    }

    if (device_config->supports_ioctl && !ops->ioctl)
    {
        bdus_set_error_dont_append_errno_(
            EINVAL,
            "The device supports \"ioctl\" requests but the driver does not"
            " implement callback 'ioctl'."
            );
    }

    // success

    return true;
}

static bool bdus_validate_attrs_rerun_(
    const struct bdus_attrs *attrs,
    const struct kbdus_device_config *device_config
    )
{
    // validate 'logical_block_size'

    if (attrs->logical_block_size != 0 &&
        attrs->logical_block_size != device_config->logical_block_size)
    {
       bdus_set_error_dont_append_errno_(
            EINVAL,
            "Invalid value %"PRIu32" for attribute 'logical_block_size', must"
            " be 0 or equal to the existing device's logical block size (which"
            " is %"PRIu32").",
            attrs->logical_block_size,
            device_config->logical_block_size
            );

        return false;
    }

    // validate 'physical_block_size'

    if (attrs->physical_block_size != 0 &&
        attrs->physical_block_size != device_config->physical_block_size)
    {
       bdus_set_error_dont_append_errno_(
            EINVAL,
            "Invalid value %"PRIu32" for attribute 'physical_block_size', must"
            " be 0 or equal to the existing device's physical block size (which"
            " is %"PRIu32").",
            attrs->physical_block_size,
            device_config->physical_block_size
            );

        return false;
    }

    // validate 'size'

    if (attrs->size != 0 && attrs->size != device_config->size)
    {
       bdus_set_error_dont_append_errno_(
            EINVAL,
            "Invalid value %"PRIu64" for attribute 'size', must be 0 or equal"
            " to the existing device's size (which is %"PRIu64").",
            attrs->size,
            device_config->size
            );

        return false;
    }

    // validate 'max_read_write_size'

    if (attrs->max_read_write_size != 0 &&
        attrs->max_read_write_size < device_config->max_read_write_size)
    {
        bdus_set_error_dont_append_errno_(
            EINVAL,
            "Invalid value %"PRIu32" for attribute 'max_read_write_size', must"
            " be 0 or greater than or equal to the original driver's value for"
            " this attribute (which is %"PRIu32").",
            attrs->max_read_write_size,
            device_config->max_read_write_size
            );

        return false;
    }

    // validate 'max_write_same_size'

    if (attrs->max_write_same_size != 0 &&
        attrs->max_write_same_size < device_config->max_write_same_size)
    {
        bdus_set_error_dont_append_errno_(
            EINVAL,
            "Invalid value %"PRIu32" for attribute 'max_write_same_size', must"
            " be 0 or greater than or equal to the original driver's value for"
            " this attribute (which is %"PRIu32").",
            attrs->max_write_same_size,
            device_config->max_write_same_size
            );

        return false;
    }

    // validate 'max_write_zeros_size'

    if (attrs->max_write_zeros_size != 0 &&
        attrs->max_write_zeros_size < device_config->max_write_zeros_size)
    {
        bdus_set_error_dont_append_errno_(
            EINVAL,
            "Invalid value %"PRIu32" for attribute 'max_write_zeros_size', must"
            " be 0 or greater than or equal to the original driver's value for"
            " this attribute (which is %"PRIu32").",
            attrs->max_write_zeros_size,
            device_config->max_write_zeros_size
            );

        return false;
    }

    // validate 'max_discard_erase_size'

    if (attrs->max_discard_erase_size != 0 &&
        attrs->max_discard_erase_size < device_config->max_discard_erase_size)
    {
        bdus_set_error_dont_append_errno_(
            EINVAL,
            "Invalid value %"PRIu32" for attribute 'max_discard_erase_size',"
            " must be 0 or greater than or equal to the original driver's value"
            " for this attribute (which is %"PRIu32").",
            attrs->max_discard_erase_size,
            device_config->max_discard_erase_size
            );

        return false;
    }

    // success

    return true;
}

static bool bdus_rerun_impl_(
    uint32_t dev_index,
    const struct bdus_ops *ops_copy,
    struct bdus_attrs *attrs_copy,
    void *user_data,
    int control_fd,
    const struct bdus_internal_config_ *adjusted_internal_config,
    const struct bdus_backend_ *backend
    )
{
    // create configuration

    attrs_copy->max_concurrent_callbacks = bdus_min_(
        bdus_max_(attrs_copy->max_concurrent_callbacks, UINT32_C(1)),
        adjusted_internal_config->max_active_queue_reqs
            + adjusted_internal_config->max_active_ioctl_reqs
        );

    struct kbdus_config kbdus_config =
    {
        .device = { 0 },

        .protocol =
        {
            .num_request_buffers = attrs_copy->max_concurrent_callbacks,
        },
    };

    strncpy(
        kbdus_config.protocol.name,
        backend->protocol_name,
        sizeof(kbdus_config.protocol.name) - 1
        );

    // get existing device's seqnum

    if (!bdus_get_dev_seqnum_(
        control_fd, dev_index, &kbdus_config.device.seqnum
        ))
    {
        return false;
    }

    // get existing device's configuration

    if (bdus_ioctl_arg_retry_(
        control_fd, KBDUS_IOCTL_GET_DEVICE_CONFIG, &kbdus_config.device
        ) != 0)
    {
        if (errno == ENODEV)
        {
            bdus_set_error_dont_append_errno_(
                errno, "The device no longer exists."
                );
        }
        else
        {
            bdus_set_error_(
                errno,
                "Failed to issue ioctl with command"
                " KBDUS_IOCTL_GET_DEVICE_CONFIGURATION to /dev/bdus-control."
                );
        }

        return false;
    }

    // validate operations and attributes

    if (!bdus_validate_ops_rerun_(ops_copy, &kbdus_config.device))
        return false;

    if (!bdus_validate_attrs_rerun_(attrs_copy, &kbdus_config.device))
        return false;

    // adjust attributes

    attrs_copy->logical_block_size   = kbdus_config.device.logical_block_size;
    attrs_copy->physical_block_size  = kbdus_config.device.physical_block_size;
    attrs_copy->size                 = kbdus_config.device.size;

    attrs_copy->max_read_write_size  = kbdus_config.device.max_read_write_size;
    attrs_copy->max_write_same_size  = kbdus_config.device.max_write_same_size;
    attrs_copy->max_write_zeros_size = kbdus_config.device.max_write_zeros_size;

    attrs_copy->max_discard_erase_size =
        kbdus_config.device.max_discard_erase_size;

    attrs_copy->disable_partition_scanning =
        kbdus_config.device.disable_partition_scanning;

    attrs_copy->recoverable = !kbdus_config.device.destroy_when_detached;

    // attach to device (don't retry ioctl to avoid race condition where device
    // is destroyed between retries, or another driver starts attaching to it)

    if (ioctl(control_fd, KBDUS_IOCTL_ATTACH_TO_DEVICE, &kbdus_config) != 0)
    {
        if (errno == EINTR)
        {
            bdus_set_error_dont_append_errno_(
                errno, "bdus_rerun() was interrupted by a signal."
                );
        }
        else if (errno == ENODEV)
        {
            bdus_set_error_dont_append_errno_(
                errno, "The device no longer exists."
                );
        }
        else if (errno == EBUSY)
        {
            bdus_set_error_dont_append_errno_(
                errno, "The device is not yet available to clients."
                );
        }
        else if (errno == EINPROGRESS)
        {
            bdus_set_error_dont_append_errno_(
                errno, "Another driver is already taking control of the device."
                );
        }
        else
        {
            bdus_set_error_(
                errno,
                "Failed to issue ioctl with command"
                " KBDUS_IOCTL_ATTACH_TO_DEVICE to /dev/bdus-control."
                );
        }

        return false;
    }

    // delegate remaining work

    return bdus_execute_driver_(
        ops_copy, attrs_copy, user_data,
        control_fd, &kbdus_config.device, adjusted_internal_config, backend,
        true
        );
}

BDUS_EXPORT_ bool bdus_rerun(
    uint32_t dev_index,
    const struct bdus_ops *ops,
    const struct bdus_attrs *attrs,
    void *user_data
    )
{
    const struct bdus_internal_config_ internal_config = { 0 };

    return bdus_rerun_with_internal_config_(
        dev_index, ops, attrs, user_data, &internal_config
        );
}

BDUS_EXPORT_ bool bdus_rerun_with_internal_config_(
    uint32_t dev_index,
    const struct bdus_ops *ops,
    const struct bdus_attrs *attrs,
    void *user_data,
    const struct bdus_internal_config_ *internal_config
    )
{
    // copy operations and attributes

    const struct bdus_ops ops_copy = *ops;
    struct bdus_attrs attrs_copy = *attrs;

    // adjust internal configuration and lookup backend

    struct bdus_internal_config_ adjusted_internal_config = *internal_config;

    const struct bdus_backend_ *const backend = bdus_adjust_internal_config_(
        &adjusted_internal_config
        );

    if (!backend)
        return false;

    // open control device

    const int control_fd = bdus_open_control_(true);

    if (control_fd < 0)
        return false;

    // delegate remaining work

    const bool success = bdus_rerun_impl_(
        dev_index, &ops_copy, &attrs_copy, user_data,
        control_fd, &adjusted_internal_config, backend
        );

    // close control device

    bdus_close_keep_errno_(control_fd);

    // return success indication

    return success;
}

/* -------------------------------------------------------------------------- */
/* device management */

static bool bdus_flush_dev_(int control_fd, uint64_t dev_seqnum)
{
    // flush device

    const int ret = bdus_ioctl_arg_retry_(
        control_fd,
        KBDUS_IOCTL_FLUSH_DEVICE,
        &dev_seqnum
        );

    // check error

    if (ret == -1)
    {
        bdus_set_error_(errno, "Failed to flush the device.");
        return false;
    }

    // success

    return true;
}

static bool bdus_request_dev_destruction_(int control_fd, uint64_t dev_seqnum)
{
    // request device destruction

    const int ret = bdus_ioctl_arg_retry_(
        control_fd,
        KBDUS_IOCTL_REQUEST_DEVICE_DESTRUCTION,
        &dev_seqnum
        );

    // check error

    if (ret == -1)
    {
        bdus_set_error_(
            errno,
            "Failed to issue ioctl with command"
            " KBDUS_IOCTL_REQUEST_DEVICE_DESTRUCTION to /dev/bdus-control."
            );

        return false;
    }

    // success

    return true;
}

static bool bdus_wait_until_dev_destroyed_(int control_fd, uint64_t dev_seqnum)
{
    // wait until device is destroyed

    const int ret = bdus_ioctl_arg_retry_(
        control_fd,
        KBDUS_IOCTL_WAIT_UNTIL_DEVICE_IS_DESTROYED,
        &dev_seqnum
        );

    // check error

    if (ret == -1)
    {
        bdus_set_error_(
            errno,
            "Failed to issue ioctl with command"
            " KBDUS_IOCTL_WAIT_UNTIL_DEVICE_IS_DESTROYED to /dev/bdus-control."
            );

        return false;
    }

    // success

    return true;
}

static bool bdus_manage_dev_(uint32_t dev_index, bool flush, bool destroy)
{
    // open control device

    const int control_fd = bdus_open_control_(true);

    if (control_fd < 0)
        return false;

    // perform management operations on device

    uint64_t dev_seqnum = UINT64_MAX; // (initialize to avoid compiler warning)

    bool success = bdus_get_dev_seqnum_(control_fd, dev_index, &dev_seqnum);

    if (flush)
    {
        success =
            success
            && bdus_flush_dev_(control_fd, dev_seqnum);
    }

    if (destroy)
    {
        success =
            success
            && bdus_request_dev_destruction_(control_fd, dev_seqnum)
            && bdus_wait_until_dev_destroyed_(control_fd, dev_seqnum);
    }

    // close control device

    bdus_close_keep_errno_(control_fd);

    // return success indicator

    return success;
}

BDUS_EXPORT_ bool bdus_flush_dev(uint32_t dev_index)
{
    return bdus_manage_dev_(dev_index, true, false);
}

BDUS_EXPORT_ bool bdus_destroy_dev(uint32_t dev_index, bool flush_dev)
{
    return bdus_manage_dev_(dev_index, flush_dev, true);
}

/* -------------------------------------------------------------------------- */
/* device indices and paths */

BDUS_EXPORT_ size_t bdus_dev_index_to_path(
    char *path_buf, size_t path_buf_size,
    uint32_t dev_index
    )
{
    // compute path

    const int ret = snprintf(
        path_buf, path_buf_size,
        "/dev/bdus-%"PRIu32, dev_index
        );

    // check for output error

    if (ret <= 0)
    {
        bdus_set_error_dont_append_errno_(EIO, "snprintf() failed.");
        return 0;
    }

    // check if path was truncated

    if (path_buf_size != 0 && (size_t)ret >= path_buf_size)
    {
        bdus_set_error_dont_append_errno_(
            EINVAL,
            "Device path has size %d (including null terminator) but buffer has"
            " size %zu.",
            ret + 1, path_buf_size
            );

        return 0;
    }

    // success

    return (size_t)ret;
}

static bool bdus_parse_uint32_(uint32_t *out_value, const char *str)
{
    if (str[0] == '\0' || str[0] == '+' || str[0] == '-' || isspace(str[0]))
        return false; // empty string or starts with a sign or whitespace char

    if (str[0] == '0' && str[1] != '\0')
        return false; // initial zero disallowed

    char *str_end;
    const unsigned long long value = strtoull(str, &str_end, 10);

    if (value == ULLONG_MAX)
        return false; // parsing failed

    if (str_end[0] != '\0')
        return false; // didn't consume entire string

    if (value > (unsigned long long)UINT32_MAX)
        return false; // value is too large

    *out_value = (uint32_t)value;
    return true;
}

static bool bdus_dev_path_to_index_(
    uint32_t *out_dev_index,
    const char *dev_path
    )
{
    const char prefix[] = "/dev/bdus-";

    // ensure that string has right prefix

    if (strncmp(dev_path, prefix, sizeof(prefix) - 1) != 0)
        return false;

    // try to parse the device index

    return bdus_parse_uint32_(out_dev_index, dev_path + sizeof(prefix) - 1);
}

BDUS_EXPORT_ bool bdus_dev_path_to_index(
    uint32_t *out_dev_index,
    const char *dev_path
    )
{
    if (bdus_dev_path_to_index_(out_dev_index, dev_path))
    {
        return true;
    }
    else
    {
        bdus_set_error_dont_append_errno_(
            EINVAL, "Invalid device path '%s'.", dev_path
            );

        return false;
    }
}

BDUS_EXPORT_ bool bdus_dev_index_or_path_to_index(
    uint32_t *out_dev_index,
    const char *dev_path_or_index
    )
{
    // try to parse as index

    if (bdus_parse_uint32_(out_dev_index, dev_path_or_index))
        return true;

    // try to parse as path

    if (bdus_dev_path_to_index_(out_dev_index, dev_path_or_index))
        return true;

    // failure

    bdus_set_error_dont_append_errno_(
        EINVAL,
        "Invalid device index or path '%s'.",
        dev_path_or_index
        );

    return false;
}

/* -------------------------------------------------------------------------- */
/* errors */

BDUS_EXPORT_ const char *bdus_get_error_message(void)
{
    return bdus_get_error_message_();
}

/* -------------------------------------------------------------------------- */
/* version */

static const struct bdus_version bdus_libbdus_version_ =
{
    (uint32_t)BDUS_VERSION_MAJOR,
    (uint32_t)BDUS_VERSION_MINOR,
    (uint32_t)BDUS_VERSION_PATCH
};

BDUS_EXPORT_ const struct bdus_version *bdus_get_libbdus_version(void)
{
    return &bdus_libbdus_version_;
}

BDUS_EXPORT_ bool bdus_get_kbdus_version(struct bdus_version *out_kbdus_version)
{
    // open control device

    const int control_fd = bdus_open_control_(false);

    if (control_fd < 0)
        return false;

    // get kbdus' version

    struct kbdus_version kbdus_ver;
    bool success;

    if (bdus_ioctl_arg_retry_(
        control_fd, KBDUS_IOCTL_GET_VERSION, &kbdus_ver
        ) == 0)
    {
        out_kbdus_version->major = kbdus_ver.major;
        out_kbdus_version->minor = kbdus_ver.minor;
        out_kbdus_version->patch = kbdus_ver.patch;

        success = true;
    }
    else
    {
        bdus_set_error_(
            errno,
            "Failed to issue ioctl with command KBDUS_IOCTL_GET_VERSION to"
            " /dev/bdus-control."
            );

        success = false;
    }

    // close control device

    bdus_close_keep_errno_(control_fd);

    // return success indication

    return success;
}

/* -------------------------------------------------------------------------- */
