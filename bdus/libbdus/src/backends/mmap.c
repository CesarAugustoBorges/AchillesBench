/* -------------------------------------------------------------------------- */
/* includes */

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200112L

#include <bdus.h>
#include <libbdus/backend.h>
#include <libbdus/backends/mmap.h>
#include <libbdus/utilities.h>

#include <errno.h>
#include <kbdus.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

/* -------------------------------------------------------------------------- */
/* private -- constants */

enum
{
    BDUSMMAP_STATUS_DEVICE_AVAILABLE_,
    BDUSMMAP_STATUS_TERMINATE_,
    BDUSMMAP_STATUS_ERROR_,
};

/* -------------------------------------------------------------------------- */
/* private -- types */

struct bdusmmap_context_
{
    uint64_t dev_seqnum;
    struct bdus_dev *dev;
    int control_fd;

    pthread_t thread;
    size_t thread_index;
    union kbdusmmap_reply_and_request *rar;
    void *payload_buffer;
    bool allow_device_available_reqs;

    int status;
    int error_errno;
    const char *error_message;
};

/* -------------------------------------------------------------------------- */
/* private -- functions -- helpers */

static size_t bdusmmap_get_payload_buffer_size_(
    const struct bdus_dev *dev,
    size_t page_size
    )
{
    // compute maximum payload size

    size_t size = (size_t)dev->attrs->max_read_write_size;

    if (dev->ops->write_same)
        size = bdus_max_(size, (size_t)dev->attrs->logical_block_size);

    if (dev->ops->ioctl)
        size = bdus_max_(size, (size_t)1 << 14);

    // round up to the page size (so that contiguous buffers are page-aligned)

    if (size > 0)
        size = ((size - 1) | (page_size - 1)) + 1;

    // return result

    return size;
}

static void bdusmmap_request_termination_(
    const struct bdusmmap_context_ *context
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

static bool bdusmmap_send_reply_and_receive_request_(
    struct bdusmmap_context_ *context
    )
{
    const int ret = bdus_ioctl_arg_retry_(
        context->control_fd,
        KBDUSMMAP_IOCTL_SEND_REPLY_AND_RECEIVE_REQUEST,
        (void *)context->thread_index
        );

    if (ret == 0)
    {
        return true;
    }
    else
    {
        context->status        = BDUSMMAP_STATUS_ERROR_;
        context->error_errno   = errno;
        context->error_message =
            "Failed to issue ioctl with command"
            " KBDUSMMAP_IOCTL_SEND_REPLY_AND_RECEIVE_REQUEST to"
            " /dev/bdus-control.";

        return false;
    }
}

static bool bdusmmap_process_request_(
    struct bdusmmap_context_ *context
    )
{
    ssize_t reply_payload_size;

    switch (context->rar->request.type)
    {
    case KBDUS_REQUEST_TYPE_DEVICE_AVAILABLE:

        if (context->allow_device_available_reqs)
        {
            context->status = BDUSMMAP_STATUS_DEVICE_AVAILABLE_;
        }
        else
        {
            context->status        = BDUSMMAP_STATUS_ERROR_;
            context->error_errno   = EINVAL;
            context->error_message =
                "Received \"device available\" notification more than once.";
        }

        return false;

    case KBDUS_REQUEST_TYPE_TERMINATE:

        context->status = BDUSMMAP_STATUS_TERMINATE_;

        return false;

    case KBDUS_REQUEST_TYPE_FLUSH_AND_TERMINATE:

        reply_payload_size = bdus_backend_process_flush_request_(
            context->dev, (int)context->thread_index,
            &context->rar->reply.negated_errno
            );

        if (reply_payload_size < 0)
        {
            context->status        = BDUSMMAP_STATUS_ERROR_;
            context->error_errno   = EINVAL;
            context->error_message = "Received request of unknown type.";
        }
        else
        {
            context->status = BDUSMMAP_STATUS_TERMINATE_;
        }

        return false;

    default:

        reply_payload_size = bdus_backend_process_request_(
            context->dev, (int)context->thread_index, context->payload_buffer,
            context->rar->request.type, context->rar->request.arg64,
            context->rar->request.arg32, &context->rar->reply.negated_errno
            );

        if (reply_payload_size < 0)
        {
            context->status        = BDUSMMAP_STATUS_ERROR_;
            context->error_errno   = EINVAL;
            context->error_message = "Received request of unknown type.";

            return false;
        }

        return true;
    }
}

static void bdusmmap_service_loop_(struct bdusmmap_context_ *context)
{
    context->rar->common.handle_index = UINT16_C(0);

    // receive requests, process them, and send replies

    while (true)
    {
        if (!bdusmmap_send_reply_and_receive_request_(context))
            break; // error

        if (!bdusmmap_process_request_(context))
            break; // error or received notification request
    }

    // check if error occurred

    if (context->status == BDUSMMAP_STATUS_ERROR_)
    {
        // service loop failed, request immediate device termination so that
        // other service threads terminate

        bdusmmap_request_termination_(context);
    }
}

static void *bdusmmap_service_loop_void_(void *context)
{
    bdusmmap_service_loop_(context);
    return NULL;
}

/* -------------------------------------------------------------------------- */
/* private -- functions -- backend entry point */

static bool bdusmmap_run_3_(
    struct bdusmmap_context_ *contexts,
    size_t num_threads
    )
{
    // run multi-threaded until device terminates or error

    for (size_t i = 1; i < num_threads; ++i)
    {
        const int ret = pthread_create(
            &contexts[i].thread,
            NULL,
            bdusmmap_service_loop_void_,
            &contexts[i]
            );

        if (ret != 0)
        {
            bdusmmap_request_termination_(&contexts[i]);

            for ( ; i > 1; --i)
                pthread_join(contexts[i - 1].thread, NULL);

            bdus_set_error_(ret, "pthread_create() failed.");
            return false;
        }
    }

    // run first service thread

    bdusmmap_service_loop_(&contexts[0]);

    // join service threads

    for (size_t i = 1; i < num_threads; ++i)
        pthread_join(contexts[i].thread, NULL);

    // check if any thread failed

    for (size_t i = 0; i < num_threads; ++i)
    {
        if (contexts[i].status == BDUSMMAP_STATUS_ERROR_)
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

static bool bdusmmap_run_2_(
    struct bdus_dev *dev,
    struct bdusmmap_context_ *contexts,
    size_t num_threads
    )
{
    // run single-threaded until device becomes available, terminates, or error

    {
        struct bdusmmap_context_ *const context = &contexts[0];

        context->allow_device_available_reqs = true;

        bdusmmap_service_loop_(&contexts[0]);

        switch (context->status)
        {
        case BDUSMMAP_STATUS_DEVICE_AVAILABLE_:
            break;

        case BDUSMMAP_STATUS_TERMINATE_:
            return true;

        case BDUSMMAP_STATUS_ERROR_:
            bdus_set_error_(context->error_errno, "%s", context->error_message);
            return false;
        }

        context->allow_device_available_reqs = false;

        // invoke on_device_available() callback and daemonize the current
        // process

        if (!bdus_backend_on_device_available_(dev, (int)context->thread_index))
            return false;
    }

    // multi-threaded

    return bdusmmap_run_3_(contexts, num_threads);
}

static bool bdusmmap_run_(
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

    // allocate and initialize context structures

    struct bdusmmap_context_ *const contexts = malloc(
        num_threads * sizeof(*contexts)
        );

    if (!contexts)
    {
        bdus_set_error_(errno, "malloc() failed.");
        return false;
    }

    // map memory

    const size_t payload_buffer_size =
        bdusmmap_get_payload_buffer_size_(dev, page_size);

    const size_t shared_memory_length =
        page_size + num_threads * payload_buffer_size;

    void *const shared_memory = mmap(
        NULL,
        shared_memory_length,
        PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_POPULATE,
        control_fd,
        0
        );

    if (shared_memory == MAP_FAILED)
    {
        free(contexts);

        bdus_set_error_(
            errno,
            "mmap() of size %zu failed.",
            shared_memory_length
            );

        return false;
    }

    for (size_t i = 0; i < num_threads; ++i)
    {
        struct bdusmmap_context_ *const c = &contexts[i];

        c->dev_seqnum = dev_seqnum;
        c->dev        = dev;
        c->control_fd = control_fd;

        c->thread_index = i;
        c->allow_device_available_reqs = false;

        c->rar =
            (union kbdusmmap_reply_and_request *)
            ((char *)shared_memory + 64 * i);

        c->payload_buffer =
            (char *)shared_memory + (size_t)page_size + i * payload_buffer_size;
    }

    // delegate things

    const bool success = bdusmmap_run_2_(dev, contexts, num_threads);

    // free context structures and unmap memory

    free(contexts);

    if (munmap(shared_memory, shared_memory_length) != 0)
    {
        bdus_set_error_(errno, "munmap() failed.");
        return false;
    }

    // return success indication

    return success;
}

/* -------------------------------------------------------------------------- */
/* interface */

const struct bdus_backend_ bdusmmap_backend_ =
{
    .backend_name  = "mmap",
    .protocol_name = "mmap",

    .run           = bdusmmap_run_,
};

/* -------------------------------------------------------------------------- */
