/* -------------------------------------------------------------------------- */
/* includes */

#include <kbdus.h>
#include <kbdus/config.h>
#include <kbdus/inverter.h>
#include <kbdus/protocol.h>
#include <kbdus/protocols/mmap.h>
#include <kbdus/utilities.h>

#include <linux/blkdev.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/vmalloc.h>

/* -------------------------------------------------------------------------- */
/* protocol state and utilities */

struct kbdusmmap_state_
{
    size_t num_request_buffers;
    size_t payload_buffer_size;

    void *shared_memory;
};

static union kbdusmmap_reply_and_request *kbdusmmap_get_rar_(
    const struct kbdusmmap_state_ *state,
    unsigned long buffer_index
    )
{
    return state->shared_memory + 64 * buffer_index;
}

static void *kbdusmmap_get_payload_buffer_(
    const struct kbdusmmap_state_ *state,
    unsigned long buffer_index
    )
{
    return
        state->shared_memory
        + PAGE_SIZE
        + state->payload_buffer_size * buffer_index;
}

static size_t kbdusmmap_get_payload_buffer_size_(
    const struct kbdus_device_config *device_config
    )
{
    size_t size;

    // compute maximum payload size

    size = (size_t)device_config->max_read_write_size;

    if (device_config->supports_write_same)
        size = max(size, (size_t)device_config->logical_block_size);

    if (device_config->supports_ioctl)
        size = max(size, (size_t)1 << 14);

    // return result rounded up to the page size (so buffers are page-aligned)

    return round_up(size, PAGE_SIZE);
}

/* -------------------------------------------------------------------------- */
/* request sending and reply receiving */

static int kbdusmmap_receive_request_(
    struct kbdus_inverter *inverter,
    struct kbdusmmap_request *request,
    void *payload_buffer
    )
{
    const struct kbdus_inverter_request *p_req;
    struct bio_vec bvec;
    struct req_iterator req_iter;
    void *bvec_mapped_page;

    // get request to be processed

    p_req = kbdus_inverter_begin_request_get(inverter);

    if (IS_ERR(p_req))
        return PTR_ERR(p_req);

    // put request into the rar's request structure

    request->handle_seqnum = p_req->handle_seqnum;
    request->handle_index  = p_req->handle_index;
    request->type          = p_req->type;

    // put request data into the payload buffer

    switch (p_req->type)
    {
    case KBDUS_REQUEST_TYPE_WRITE:
    case KBDUS_REQUEST_TYPE_WRITE_SAME:
    case KBDUS_REQUEST_TYPE_FUA_WRITE:

        rq_for_each_segment(bvec, p_req->queue_req, req_iter)
        {
            bvec_mapped_page = kmap(bvec.bv_page);

            memcpy(
                payload_buffer,
                bvec_mapped_page + bvec.bv_offset,
                bvec.bv_len
                );

            kunmap(bvec_mapped_page);

            payload_buffer += bvec.bv_len;
        }

        /* fallthrough */

    case KBDUS_REQUEST_TYPE_READ:
    case KBDUS_REQUEST_TYPE_WRITE_ZEROS_NO_UNMAP:
    case KBDUS_REQUEST_TYPE_WRITE_ZEROS_MAY_UNMAP:
    case KBDUS_REQUEST_TYPE_DISCARD:
    case KBDUS_REQUEST_TYPE_SECURE_ERASE:

        request->arg64 = 512ull * (u64)blk_rq_pos(p_req->queue_req);
        request->arg32 = (u32)blk_rq_bytes(p_req->queue_req);

        break;

    case KBDUS_REQUEST_TYPE_IOCTL:

        request->arg32 = (u32)p_req->ioctl_req_command;

        if (_IOC_DIR(p_req->ioctl_req_command) & _IOC_READ)
        {
            memcpy(
                payload_buffer,
                p_req->ioctl_req_argument_buffer,
                (size_t)_IOC_SIZE(p_req->ioctl_req_command)
                );
        }

        break;
    }

    // commit "request get"

    kbdus_inverter_commit_request_get(inverter, p_req);

    // success

    return 0;
}

static int kbdusmmap_send_reply_(
    struct kbdus_inverter *inverter,
    const struct kbdusmmap_reply *reply,
    const void *payload_buffer
    )
{
    const struct kbdus_inverter_request *p_req;
    struct bio_vec bvec;
    struct req_iterator req_iter;
    void *bvec_mapped_page;

    // check if there is a reply at all

    if (reply->handle_index == 0)
        return 0;

    // get request from handle

    p_req = kbdus_inverter_begin_request_completion(
        inverter, reply->handle_index, reply->handle_seqnum
        );

    if (!p_req)
        return 0; // request timed out, was canceled, or was already completed

    if (IS_ERR(p_req))
        return PTR_ERR(p_req);

    // copy data if applicable and request succeeded

    if (reply->negated_errno == 0)
    {
        switch (p_req->type)
        {
        case KBDUS_REQUEST_TYPE_READ:

            rq_for_each_segment(bvec, p_req->queue_req, req_iter)
            {
                bvec_mapped_page = kmap(bvec.bv_page);

                memcpy(
                    bvec_mapped_page + bvec.bv_offset,
                    payload_buffer,
                    bvec.bv_len
                    );

                kunmap(bvec_mapped_page);

                payload_buffer += bvec.bv_len;
            }

            break;

        case KBDUS_REQUEST_TYPE_IOCTL:

            if (_IOC_DIR(p_req->ioctl_req_command) & _IOC_WRITE)
            {
                memcpy(
                    p_req->ioctl_req_argument_buffer,
                    payload_buffer,
                    (size_t)_IOC_SIZE(p_req->ioctl_req_command)
                    );
            }

            break;
        }
    }

    // commit request completion and succeed

    kbdus_inverter_commit_request_completion(
        inverter, p_req, (int)reply->negated_errno
        );

    // success

    return 0;
}

static int kbdusmmap_send_reply_and_receive_request_(
    struct kbdus_inverter *inverter,
    union kbdusmmap_reply_and_request *rar,
    void *payload_buffer
    )
{
    int ret;

    // handle reply (if there is a reply at all)

    ret = kbdusmmap_send_reply_(inverter, &rar->reply, payload_buffer);

    if (ret != 0)
        return ret;

    // receive request

    return kbdusmmap_receive_request_(
        inverter, &rar->request, payload_buffer
        );
}

/* -------------------------------------------------------------------------- */
/* protocol implementation */

static bool kbdusmmap_validate_config_(const struct kbdus_config *config)
{
    return
        config->protocol.num_request_buffers >= 1 &&
        config->protocol.num_request_buffers <= KBDUSMMAP_MAX_REQUEST_BUFFERS;
}

static void *kbdusmmap_create_instance_(const struct kbdus_config *config)
{
    size_t payload_buffer_size;
    struct kbdusmmap_state_ *state;

    // TODO: document

    state = kmalloc(sizeof(*state), GFP_KERNEL);

    // TODO: document

    if (!state)
        return ERR_PTR(-ENOMEM);

    state->num_request_buffers = config->protocol.num_request_buffers;

    state->payload_buffer_size =
        kbdusmmap_get_payload_buffer_size_(&config->device);

    // TODO: document

    state->shared_memory = vmalloc_user(
        PAGE_SIZE
        + state->num_request_buffers * state->payload_buffer_size
        );

    if (!state->shared_memory)
    {
        kfree(state);
        return ERR_PTR(-ENOMEM);
    }

    // success

    return state;
}

static void kbdusmmap_destroy_instance_(void *instance_data)
{
    struct kbdusmmap_state_ *state;

    state = instance_data;

    vfree(state->shared_memory);
    kfree(state);
}

static int kbdusmmap_handle_control_mmap_(
    void *instance_data,
    struct kbdus_inverter *inverter,
    struct vm_area_struct *vma
    )
{
    struct kbdusmmap_state_ *state;

    state = instance_data;

    return remap_vmalloc_range(vma, state->shared_memory, vma->vm_pgoff);
}

static int kbdusmmap_handle_control_ioctl_(
    void *instance_data,
    struct kbdus_inverter *inverter,
    unsigned int command,
    void __user *argument_usrptr
    )
{
    const struct kbdusmmap_state_ *state;
    unsigned long buffer_index;
    union kbdusmmap_reply_and_request *rar;
    void *payload_buffer;

    // validate command

    switch (command)
    {
    case KBDUSMMAP_IOCTL_RECEIVE_REQUEST:
    case KBDUSMMAP_IOCTL_SEND_REPLY:
    case KBDUSMMAP_IOCTL_SEND_REPLY_AND_RECEIVE_REQUEST:
        break;

    default:
        return -ENOTTY;
    }

    // validate argument

    state = instance_data;
    buffer_index = (unsigned long)argument_usrptr;

    if (buffer_index >= state->num_request_buffers)
        return -EINVAL;

    rar = kbdusmmap_get_rar_(state, buffer_index);
    payload_buffer = kbdusmmap_get_payload_buffer_(state, buffer_index);

    // delegate processing to appropriate function

    switch (command)
    {
    case KBDUSMMAP_IOCTL_RECEIVE_REQUEST:
        return kbdusmmap_receive_request_(
            inverter, &rar->request, payload_buffer
            );

    case KBDUSMMAP_IOCTL_SEND_REPLY:
        return kbdusmmap_send_reply_(
            inverter, &rar->reply, payload_buffer
            );

    case KBDUSMMAP_IOCTL_SEND_REPLY_AND_RECEIVE_REQUEST:
        return kbdusmmap_send_reply_and_receive_request_(
            inverter, rar, payload_buffer
            );

    default:
        kbdus_assert_if_debug(false);
        return -ENOTTY;
    }
}

/* -------------------------------------------------------------------------- */
/* subcomponent interface */

const struct kbdus_protocol kbdusmmap_protocol =
{
    .name                 = "mmap",

    .validate_config      = kbdusmmap_validate_config_,

    .create_instance      = kbdusmmap_create_instance_,
    .destroy_instance     = kbdusmmap_destroy_instance_,

    .handle_control_mmap  = kbdusmmap_handle_control_mmap_,
    .handle_control_ioctl = kbdusmmap_handle_control_ioctl_,
};

/* -------------------------------------------------------------------------- */
