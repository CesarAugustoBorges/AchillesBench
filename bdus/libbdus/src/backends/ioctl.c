/* -------------------------------------------------------------------------- */
/* includes */

#define _POSIX_C_SOURCE 200112L

#include <bdus.h>
#include <libbdus/backend.h>
#include <libbdus/backends/ioctl.h>
#include <libbdus/utilities.h>

#include <errno.h>
#include <kbdus.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

/* -------------------------------------------------------------------------- */
/* private -- constants */

enum
{
    BDUSIOCTL_STATUS_DEVICE_AVAILABLE_,
    BDUSIOCTL_STATUS_TERMINATE_,
    BDUSIOCTL_STATUS_ERROR_,
};

/* -------------------------------------------------------------------------- */
/* private -- types */

struct bdusioctl_context_
{
    uint64_t dev_seqnum;
    struct bdus_dev *dev;
    int control_fd;

    pthread_t thread;
    int thread_index;
    void *payload_buffer;
    bool allow_device_available_reqs;

    int status;
    int error_errno;
    const char *error_message;
};

/* -------------------------------------------------------------------------- */
/* private -- functions -- helpers */

static size_t bdusioctl_get_payload_buffer_size_(const struct bdus_dev *dev)
{
    size_t size = (size_t)dev->attrs->max_read_write_size;

    if (dev->ops->write_same)
        size = bdus_max_(size, (size_t)dev->attrs->logical_block_size);

    if (dev->ops->ioctl)
        size = bdus_max_(size, (size_t)1 << 14);

    return size;
}

static void bdusioctl_request_termination_(
    const struct bdusioctl_context_ *context
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

static bool bdusioctl_send_reply_and_receive_request_(
    struct bdusioctl_context_ *context,
    union kbdusioctl_reply_and_request *rar
    )
{
    const int ret = bdus_ioctl_arg_retry_(
        context->control_fd,
        KBDUSIOCTL_IOCTL_SEND_REPLY_AND_RECEIVE_REQUEST,
        rar
        );

    if (ret == 0)
    {
        return true;
    }
    else
    {
        context->status        = BDUSIOCTL_STATUS_ERROR_;
        context->error_errno   = errno;
        context->error_message =
            "Failed to issue ioctl with command"
            " KBDUSIOCTL_IOCTL_SEND_REPLY_AND_RECEIVE_REQUEST to"
            " /dev/bdus-control.";

        return false;
    }
}

static bool bdusioctl_process_request_(
    struct bdusioctl_context_ *context,
    union kbdusioctl_reply_and_request *rar
    )
{
    ssize_t reply_payload_size;

    switch (rar->request.type)
    {
    case KBDUS_REQUEST_TYPE_DEVICE_AVAILABLE:

        if (context->allow_device_available_reqs)
        {
            context->status = BDUSIOCTL_STATUS_DEVICE_AVAILABLE_;
        }
        else
        {
            context->status        = BDUSIOCTL_STATUS_ERROR_;
            context->error_errno   = EINVAL;
            context->error_message =
                "Received \"device available\" notification more than once.";
        }

        return false;

    case KBDUS_REQUEST_TYPE_TERMINATE:

        context->status = BDUSIOCTL_STATUS_TERMINATE_;

        return false;

    case KBDUS_REQUEST_TYPE_FLUSH_AND_TERMINATE:

        reply_payload_size = bdus_backend_process_flush_request_(
            context->dev, context->thread_index, &rar->reply.negated_errno
            );

        if (reply_payload_size < 0)
        {
            context->status        = BDUSIOCTL_STATUS_ERROR_;
            context->error_errno   = EINVAL;
            context->error_message = "Received request of unknown type.";
        }
        else
        {
            context->status = BDUSIOCTL_STATUS_TERMINATE_;
        }

        return false;

    default:

        reply_payload_size = bdus_backend_process_request_(
            context->dev, context->thread_index, context->payload_buffer,
            rar->request.type, rar->request.arg64, rar->request.arg32,
            &rar->reply.negated_errno
            );

        if (reply_payload_size < 0)
        {
            context->status        = BDUSIOCTL_STATUS_ERROR_;
            context->error_errno   = EINVAL;
            context->error_message = "Received request of unknown type.";

            return false;
        }

        return true;
    }
}

static void bdusioctl_service_loop_(struct bdusioctl_context_ *context)
{
    union kbdusioctl_reply_and_request rar =
    {
        .common =
        {
            .buffer_ptr   = (uint64_t)context->payload_buffer,
            .handle_index = UINT16_C(0),
        },
    };

    // receive requests, process them, and send replies

    while (true)
    {
        if (!bdusioctl_send_reply_and_receive_request_(context, &rar))
            break; // error

        if (!bdusioctl_process_request_(context, &rar))
            break; // error or received notification request
    }

    // check if error occurred

    if (context->status == BDUSIOCTL_STATUS_ERROR_)
    {
        // service loop failed, request immediate device termination so that
        // other service threads terminate

        bdusioctl_request_termination_(context);
    }
}

static void *bdusioctl_service_loop_void_(void *context)
{
    bdusioctl_service_loop_(context);
    return NULL;
}

/* -------------------------------------------------------------------------- */
/* private -- functions -- backend entry point */

static bool bdusioctl_run_3_(
    struct bdusioctl_context_ *contexts,
    size_t num_threads
    )
{
    // run multi-threaded until device terminates or error

    for (size_t i = 1; i < num_threads; ++i)
    {
        const int ret = pthread_create(
            &contexts[i].thread,
            NULL,
            bdusioctl_service_loop_void_,
            &contexts[i]
            );

        if (ret != 0)
        {
            bdusioctl_request_termination_(&contexts[i]);

            for ( ; i > 1; --i)
                pthread_join(contexts[i - 1].thread, NULL);

            bdus_set_error_(ret, "pthread_create() failed.");
            return false;
        }
    }

    // run first service thread

    bdusioctl_service_loop_(&contexts[0]);

    // join service threads

    for (size_t i = 1; i < num_threads; ++i)
        pthread_join(contexts[i].thread, NULL);

    // check if any thread failed

    for (size_t i = 0; i < num_threads; ++i)
    {
        if (contexts[i].status == BDUSIOCTL_STATUS_ERROR_)
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

static bool bdusioctl_run_2_(
    struct bdus_dev *dev,
    struct bdusioctl_context_ *contexts,
    size_t num_threads
    )
{
    // run single-threaded until device becomes available, terminates, or error

    {
        struct bdusioctl_context_ *const context = &contexts[0];

        context->allow_device_available_reqs = true;

        bdusioctl_service_loop_(&contexts[0]);

        switch (context->status)
        {
        case BDUSIOCTL_STATUS_DEVICE_AVAILABLE_:
            break;

        case BDUSIOCTL_STATUS_TERMINATE_:
            return true;

        case BDUSIOCTL_STATUS_ERROR_:
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

    return bdusioctl_run_3_(contexts, num_threads);
}

static bool bdusioctl_run_(
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

    const size_t payload_buffer_size = bdusioctl_get_payload_buffer_size_(dev);

    // allocate and initialize context structures and payload buffers

    struct bdusioctl_context_ *const contexts = malloc(
        num_threads * sizeof(*contexts)
        );

    if (!contexts)
    {
        bdus_set_error_(errno, "malloc() failed.");
        return false;
    }

    for (size_t i = 0; i < num_threads; ++i)
    {
        struct bdusioctl_context_ *const c = &contexts[i];

        c->dev_seqnum = dev_seqnum;
        c->dev        = dev;
        c->control_fd = control_fd;

        c->thread_index = (int)i;
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

    const bool success = bdusioctl_run_2_(dev, contexts, num_threads);

    // free context structures and payload buffers

    for (size_t i = 0; i < num_threads; ++i)
        free(contexts[i].payload_buffer);

    free(contexts);

    // return success indication

    return success;
}

/* -------------------------------------------------------------------------- */
/* interface */

const struct bdus_backend_ bdusioctl_backend_ =
{
    .backend_name  = "ioctl",
    .protocol_name = "ioctl",

    .run           = bdusioctl_run_,
};

/* -------------------------------------------------------------------------- */
