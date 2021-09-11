/* -------------------------------------------------------------------------- */
/* includes */

#include <bdus.h>
#include <libbdus/backend.h>
#include <libbdus/backends/ioctl.h>
#include <libbdus/backends/mmap.h>
#include <libbdus/backends/rw.h>
#include <libbdus/utilities.h>

#include <inttypes.h>
#include <kbdus.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>

/* -------------------------------------------------------------------------- */
/* private -- constants */

static const struct bdus_backend_ *const bdus_backend_all_[] =
{
    &bdusioctl_backend_,
    &bdusmmap_backend_,
    &bdusrw_backend_,
};

static const size_t bdus_backend_all_count_ =
    sizeof(bdus_backend_all_) / sizeof(bdus_backend_all_[0]);

/* -------------------------------------------------------------------------- */

const struct bdus_backend_ *bdus_backend_lookup_(const char *backend_name)
{
    // look up backend by name

    for (size_t i = 0; i < bdus_backend_all_count_; ++i)
    {
        if (strcmp(bdus_backend_all_[i]->backend_name, backend_name) == 0)
            return bdus_backend_all_[i];
    }

    // no backend with specified name

    return NULL;
}

/* -------------------------------------------------------------------------- */

static int bdus_backend_on_device_available_default_(struct bdus_dev *dev)
{
    return (puts(dev->path) == EOF || fflush(stdout) == EOF) ? EIO : 0;
}

bool bdus_backend_on_device_available_(struct bdus_dev *dev, int thread_index)
{
    // invoke on_device_available() callback

    int ret;

    if (dev->ops->on_device_available)
    {
        if (dev->attrs->log_callbacks)
            bdus_log_thread_no_args_(thread_index, "on_device_available(dev)");

        ret = dev->ops->on_device_available(dev);
    }
    else
    {
        if (dev->attrs->log_callbacks)
        {
            bdus_log_thread_no_args_(
                thread_index,
                "on_device_available(dev) [not implemented, using default"
                " implementation]"
                );
        }

        ret = bdus_backend_on_device_available_default_(dev);
    }

    if (ret != 0)
    {
        bdus_set_error_(
            ret, "Driver callback on_device_available() failed."
            );

        return false;
    }

    // daemonize the current process

    if (!dev->attrs->dont_daemonize)
    {
        if (dev->attrs->log_callbacks)
            bdus_log_no_args_("daemonizing...");

        if (!bdus_daemonize_())
        {
            bdus_set_error_(errno, "Failed to daemonize the current process.");
            return false;
        }
    }

    // success

    return true;
}

ssize_t bdus_backend_process_request_(
    struct bdus_dev *dev,
    int thread_index,
    void *payload_buffer,
    uint32_t type,
    uint64_t arg64,
    uint32_t arg32,
    int32_t *out_negated_errno
    )
{
    switch (type)
    {
    case KBDUS_REQUEST_TYPE_READ:

        // log callback invocation

        if (dev->attrs->log_callbacks)
        {
            bdus_log_thread_(
                thread_index,
                "read(%p, %"PRIu64", %"PRIu32", dev)",
                payload_buffer, arg64, arg32
                );
        }

        // invoke 'read' callback

        *out_negated_errno = -(int32_t)dev->ops->read(
            payload_buffer, arg64, arg32, dev
            );

        return *out_negated_errno == 0 ? (ssize_t)arg32 : 0;

    case KBDUS_REQUEST_TYPE_WRITE:

        // log callback invocation

        if (dev->attrs->log_callbacks)
        {
            bdus_log_thread_(
                thread_index,
                "write(%p, %"PRIu64", %"PRIu32", dev)",
                payload_buffer, arg64, arg32
                );
        }

        // invoke 'write' callback

        *out_negated_errno = -(int32_t)dev->ops->write(
            payload_buffer, arg64, arg32, dev
            );

        return 0;

    case KBDUS_REQUEST_TYPE_WRITE_SAME:

        // log callback invocation

        if (dev->attrs->log_callbacks)
        {
            bdus_log_thread_(
                thread_index,
                "write_same(%p, %"PRIu64", %"PRIu32", dev)",
                payload_buffer, arg64, arg32
                );
        }

        // invoke 'write_same' callback

        *out_negated_errno = -(int32_t)dev->ops->write_same(
            payload_buffer, arg64, arg32, dev
            );

        return 0;

    case KBDUS_REQUEST_TYPE_WRITE_ZEROS_NO_UNMAP:

        // log callback invocation

        if (dev->attrs->log_callbacks)
        {
            bdus_log_thread_(
                thread_index,
                "write_zeros(%"PRIu64", %"PRIu32", false, dev)",
                arg64, arg32
                );
        }

        // invoke 'write_zeros' callback

        *out_negated_errno = -(int32_t)dev->ops->write_zeros(
            arg64, arg32, false, dev
            );

        return 0;

    case KBDUS_REQUEST_TYPE_WRITE_ZEROS_MAY_UNMAP:

        // log callback invocation

        if (dev->attrs->log_callbacks)
        {
            bdus_log_thread_(
                thread_index,
                "write_zeros(%"PRIu64", %"PRIu32", true, dev)",
                arg64, arg32
                );
        }

        // invoke 'write_zeros' callback

        *out_negated_errno = -(int32_t)dev->ops->write_zeros(
            arg64, arg32, true, dev
            );

        return 0;

    case KBDUS_REQUEST_TYPE_FUA_WRITE:

        // log callback invocation

        if (dev->attrs->log_callbacks)
        {
            bdus_log_thread_(
                thread_index,
                "fua_write(%p, %"PRIu64", %"PRIu32", dev)",
                payload_buffer, arg64, arg32
                );
        }

        // invoke 'fua_write' callback

        *out_negated_errno = -(int32_t)dev->ops->fua_write(
            payload_buffer, arg64, arg32, dev
            );

        return 0;

    case KBDUS_REQUEST_TYPE_FLUSH:

        // log callback invocation

        if (dev->attrs->log_callbacks)
            bdus_log_thread_no_args_(thread_index, "flush(dev)");

        // invoke 'flush' callback

        *out_negated_errno = -(int32_t)dev->ops->flush(dev);

        return 0;

    case KBDUS_REQUEST_TYPE_DISCARD:

        // log callback invocation

        if (dev->attrs->log_callbacks)
        {
            bdus_log_thread_(
                thread_index,
                "discard(%"PRIu64", %"PRIu32", dev)",
                arg64, arg32
                );
        }

        // invoke 'discard' callback

        *out_negated_errno = -(int32_t)dev->ops->discard(arg64, arg32, dev);

        return 0;

    case KBDUS_REQUEST_TYPE_SECURE_ERASE:

        // log callback invocation

        if (dev->attrs->log_callbacks)
        {
            bdus_log_thread_(
                thread_index,
                "secure_erase(%"PRIu64", %"PRIu32", dev)",
                arg64, arg32
                );
        }

        // invoke 'secure_erase' callback

        *out_negated_errno = -(int32_t)dev->ops->secure_erase(
            arg64, arg32, dev
            );

        return 0;

    case KBDUS_REQUEST_TYPE_IOCTL:

        // log callback invocation

        if (dev->attrs->log_callbacks)
        {
            if (_IOC_DIR(arg32) == _IOC_NONE)
            {
                bdus_log_thread_(
                    thread_index,
                    "ioctl(_IO(0x%"PRIX32", %"PRIu32"), NULL, dev)",
                    (uint32_t)_IOC_TYPE(arg32), (uint32_t)_IOC_NR(arg32)
                    );
            }
            else
            {
                const char *cmd_macro;

                switch (_IOC_DIR(arg32))
                {
                case _IOC_READ             : cmd_macro = "_IOR";  break;
                case _IOC_WRITE            : cmd_macro = "_IOW";  break;
                case _IOC_READ | _IOC_WRITE: cmd_macro = "_IOWR"; break;
                default                    : cmd_macro = NULL;    break;
                }

                bdus_log_thread_(
                    thread_index,
                    "ioctl(%s(0x%"PRIX32", %"PRIu32", %"PRIu32"), %p, dev)",
                    cmd_macro, (uint32_t)_IOC_TYPE(arg32),
                    (uint32_t)_IOC_NR(arg32), (uint32_t)_IOC_SIZE(arg32),
                    payload_buffer
                    );
            }
        }

        if (_IOC_DIR(arg32) == _IOC_NONE)
        {
            // invoke 'ioctl' callback

            *out_negated_errno = -(int32_t)dev->ops->ioctl(arg32, NULL, dev);

            return 0;
        }
        else
        {
            // clear payload buffer if write-only ioctl

            if (_IOC_DIR(arg32) == _IOC_WRITE)
                memset(payload_buffer, 0, (size_t)_IOC_SIZE(arg32));

            // invoke 'ioctl' callback

            *out_negated_errno = -(int32_t)dev->ops->ioctl(
                arg32, payload_buffer, dev
                );

            return
                *out_negated_errno == 0 && (_IOC_DIR(arg32) & _IOC_WRITE)
                ? (ssize_t)_IOC_SIZE(arg32)
                : 0;
        }

    default:

        // unknown request type

        return (ssize_t)(-1);
    }
}

ssize_t bdus_backend_process_flush_request_(
    struct bdus_dev *dev,
    int thread_index,
    int32_t *out_negated_errno
    )
{
    // log callback invocation

    if (dev->attrs->log_callbacks)
        bdus_log_thread_no_args_(thread_index, "flush(dev)");

    // invoke 'flush' callback

    *out_negated_errno = -(int32_t)dev->ops->flush(dev);

    return 0;
}

/* -------------------------------------------------------------------------- */
