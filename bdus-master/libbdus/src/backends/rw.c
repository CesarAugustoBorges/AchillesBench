/* -------------------------------------------------------------------------- */
/* includes */

#define _POSIX_C_SOURCE 200112L

#include <bdus.h>
#include <libbdus/backend.h>
#include <libbdus/backends/rw.h>
#include <libbdus/utilities.h>

#include <errno.h>
#include <kbdus.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <unistd.h>

/* -------------------------------------------------------------------------- */
/* private -- constants */

enum
{
    BDUSRW_STATUS_DEVICE_AVAILABLE_,
    BDUSRW_STATUS_TERMINATE_,
    BDUSRW_STATUS_ERROR_,
};

/* -------------------------------------------------------------------------- */
/* private -- types */

struct bdusrw_context_
{
    uint64_t dev_seqnum;
    struct bdus_dev *dev;
    int control_fd;

    pthread_t thread;
    int thread_index;
    void *payload_buffer;
    size_t payload_buffer_size;
    bool allow_device_available_reqs;

    int status;
    int error_errno;
    const char *error_message;
};

/* -------------------------------------------------------------------------- */
/* private -- functions -- helpers */

static size_t bdusrw_get_payload_buffer_size_(const struct bdus_dev *dev)
{
    size_t size = (size_t)dev->attrs->max_read_write_size;

    if (dev->ops->write_same)
        size = bdus_max_(size, (size_t)dev->attrs->logical_block_size);

    if (dev->ops->ioctl)
        size = bdus_max_(size, (size_t)1 << 14);

    return size;
}

static void bdusrw_request_termination_(
    const struct bdusrw_context_ *context
    )
{
    bdus_ioctl_arg_retry_(
        context->control_fd,
        KBDUS_IOCTL_REQUEST_SESSION_TERMINATION,
        (uint64_t *)&context->dev_seqnum // cast to remove const
        );
}

/* -------------------------------------------------------------------------- */
/* private -- functions -- service loop */

static bool bdusrw_read_request_(
    struct bdusrw_context_ *context,
    const struct iovec request_iovec[2]
    )
{
    while (true)
    {
        const ssize_t ret = readv(context->control_fd, request_iovec, 2);

        if (ret < 0)
        {
            if (errno != EINTR)
            {
                context->status        = BDUSRW_STATUS_ERROR_;
                context->error_errno   = errno;
                context->error_message =
                    "Failed to readv() from /dev/bdus-control.";

                return false;
            }
        }
        else if ((size_t)ret < sizeof(struct kbdusrw_request_header))
        {
            context->status        = BDUSRW_STATUS_ERROR_;
            context->error_errno   = EINVAL;
            context->error_message =
                "readv() from /dev/bdus-control read less than expected.";

            return false;
        }
        else
        {
            return true;
        }
    }
}

static bool bdusrw_write_reply_(
    struct bdusrw_context_ *context,
    const struct iovec reply_iovec[2]
    )
{
    while (true)
    {
        const ssize_t ret = writev(context->control_fd, reply_iovec, 2);

        if (ret < 0)
        {
            if (errno != EINTR)
            {
                context->status        = BDUSRW_STATUS_ERROR_;
                context->error_errno   = errno;
                context->error_message =
                    "Failed to writev() to /dev/bdus-control.";

                return false;
            }
        }
        else if (
            (size_t)ret !=
                sizeof(struct kbdusrw_reply_header) + reply_iovec[1].iov_len
            )
        {
            context->status        = BDUSRW_STATUS_ERROR_;
            context->error_errno   = EINVAL;
            context->error_message =
                "writev() to /dev/bdus-control wrote an unexpected amount of"
                " bytes.";

            return false;
        }
        else
        {
            return true;
        }
    }
}

static ssize_t bdusrw_process_request_(
    struct bdusrw_context_ *context,
    const struct kbdusrw_request_header *request,
    int32_t *out_negated_errno
    )
{
    ssize_t reply_payload_size;

    switch (request->type)
    {
    case KBDUS_REQUEST_TYPE_DEVICE_AVAILABLE:

        if (context->allow_device_available_reqs)
        {
            context->status = BDUSRW_STATUS_DEVICE_AVAILABLE_;
        }
        else
        {
            context->status        = BDUSRW_STATUS_ERROR_;
            context->error_errno   = EINVAL;
            context->error_message =
                "Received \"device available\" notification more than once.";
        }

        return -1;

    case KBDUS_REQUEST_TYPE_TERMINATE:

        context->status = BDUSRW_STATUS_TERMINATE_;

        return -1;

    case KBDUS_REQUEST_TYPE_FLUSH_AND_TERMINATE:

        reply_payload_size = bdus_backend_process_flush_request_(
            context->dev, context->thread_index, out_negated_errno
            );

        if (reply_payload_size < 0)
        {
            context->status        = BDUSRW_STATUS_ERROR_;
            context->error_errno   = EINVAL;
            context->error_message = "Received request of unknown type.";
        }
        else
        {
            context->status = BDUSRW_STATUS_TERMINATE_;
        }

        return -1;

    default:

        reply_payload_size = bdus_backend_process_request_(
            context->dev, context->thread_index, context->payload_buffer,
            request->type, request->arg64, request->arg32, out_negated_errno
            );

        if (reply_payload_size < 0)
        {
            context->status        = BDUSRW_STATUS_ERROR_;
            context->error_errno   = EINVAL;
            context->error_message = "Received request of unknown type.";
        }

        return reply_payload_size;
    }
}

static void bdusrw_service_loop_(struct bdusrw_context_ *context)
{
    struct kbdusrw_request_header request;
    struct kbdusrw_reply_header reply;

    const struct iovec request_iovec[2] = {
        { &request               , sizeof(request)              },
        { context->payload_buffer, context->payload_buffer_size },
    };

    struct iovec reply_iovec[2] = {
        { &reply                 , sizeof(reply)                },
        { context->payload_buffer, 0                            },
    };

    // read requests, process them, and write replies (if warranted)

    while (true)
    {
        if (!bdusrw_read_request_(context, request_iovec))
            break; // error

        const ssize_t reply_payload_size = bdusrw_process_request_(
            context, &request, &reply.negated_errno
            );

        if (reply_payload_size < 0)
            break; // error or received notification request

        if (request.handle_index != 0)
        {
            reply.handle_seqnum = request.handle_seqnum;
            reply.handle_index  = request.handle_index;

            reply_iovec[1].iov_len = (size_t)reply_payload_size;

            if (!bdusrw_write_reply_(context, reply_iovec))
                break; // error
        }
    }

    // check if error occurred

    if (context->status == BDUSRW_STATUS_ERROR_)
    {
        // service loop failed, request immediate device termination so that
        // other service threads terminate

        bdusrw_request_termination_(context);
    }
}

static void *bdusrw_service_loop_void_(void *context)
{
    bdusrw_service_loop_(context);
    return NULL;
}

/* -------------------------------------------------------------------------- */
/* private -- functions -- backend entry point */

static bool bdusrw_run_3_(
    struct bdusrw_context_ *contexts,
    size_t num_threads
    )
{
    // run multi-threaded until device terminates or error

    for (size_t i = 1; i < num_threads; ++i)
    {
        const int ret = pthread_create(
            &contexts[i].thread,
            NULL,
            bdusrw_service_loop_void_,
            &contexts[i]
            );

        if (ret != 0)
        {
            bdusrw_request_termination_(&contexts[i]);

            for ( ; i > 1; --i)
                pthread_join(contexts[i - 1].thread, NULL);

            bdus_set_error_(ret, "pthread_create() failed.");
            return false;
        }
    }

    // run first service thread

    bdusrw_service_loop_(&contexts[0]);

    // join service threads

    for (size_t i = 1; i < num_threads; ++i)
        pthread_join(contexts[i].thread, NULL);

    // check if any thread failed

    for (size_t i = 0; i < num_threads; ++i)
    {
        if (contexts[i].status == BDUSRW_STATUS_ERROR_)
        {
            bdus_set_error_(
                contexts[i].error_errno, "%s", contexts[i].error_message
                );

            return false;
        }
    }

    // success

    return true;
}

static bool bdusrw_run_2_(
    struct bdus_dev *dev,
    struct bdusrw_context_ *contexts,
    size_t num_threads
    )
{
    // run single-threaded until device becomes available, terminates, or error

    {
        struct bdusrw_context_ *const context = &contexts[0];

        context->allow_device_available_reqs = true;

        bdusrw_service_loop_(&contexts[0]);

        switch (context->status)
        {
        case BDUSRW_STATUS_DEVICE_AVAILABLE_:
            break;

        case BDUSRW_STATUS_TERMINATE_:
            return true;

        case BDUSRW_STATUS_ERROR_:
            bdus_set_error_(context->error_errno, "%s", context->error_message);
            return false;
        }

        context->allow_device_available_reqs = false;

        // invoke on_device_available() callback and daemonize the current
        // process

        if (!bdus_backend_on_device_available_(dev, context->thread_index))
            return false;
    }

    // multi-threaded

    return bdusrw_run_3_(contexts, num_threads);
}

static bool bdusrw_run_(
    int control_fd,
    uint64_t dev_seqnum,
    struct bdus_dev *dev,
    const struct bdus_internal_config_ *config
    )
{
    (void)config;

    const size_t num_threads = (size_t)dev->attrs->max_concurrent_callbacks;

    const size_t page_size = bdus_get_page_size_();

    if (page_size == 0)
        return false;

    const size_t payload_buffer_size = bdusrw_get_payload_buffer_size_(dev);

    // allocate and initialize context structures and payload buffers

    struct bdusrw_context_ *const contexts = malloc(
        num_threads * sizeof(*contexts)
        );

    if (!contexts)
    {
        bdus_set_error_(errno, "malloc() failed.");
        return false;
    }

    for (size_t i = 0; i < num_threads; ++i)
    {
        struct bdusrw_context_ *const c = &contexts[i];

        c->dev_seqnum = dev_seqnum;
        c->dev        = dev;
        c->control_fd = control_fd;

        c->thread_index = (int)i;
        c->payload_buffer_size = payload_buffer_size;
        c->allow_device_available_reqs = false;

        // note: each payload buffer is allocated separately (instead of doing
        // one big allocation and splitting it into several buffers) so that
        // out-of-bounds accesses are more likely to cause segmentation faults
        // instead of silent corruption bugs

        const int result = posix_memalign(
            &c->payload_buffer, page_size, payload_buffer_size
            );

        if (result != 0)
        {
            bdus_set_error_(
                result,
                "posix_memalign(%p, %zu, %zu) failed.",
                (void *)&c->payload_buffer, page_size, payload_buffer_size
                );

            for ( ; i > 0; --i)
                free(contexts[i - 1].payload_buffer);

            return false;
        }
    }

    // delegate things

    const bool success = bdusrw_run_2_(dev, contexts, num_threads);

    // free context structures and payload buffers

    for (size_t i = 0; i < num_threads; ++i)
        free(contexts[i].payload_buffer);

    free(contexts);

    // return success indication

    return success;
}

/* -------------------------------------------------------------------------- */
/* interface */

const struct bdus_backend_ bdusrw_backend_ =
{
    .backend_name  = "rw",
    .protocol_name = "rw",

    .run           = bdusrw_run_,
};

/* -------------------------------------------------------------------------- */
