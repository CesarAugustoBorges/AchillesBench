/* -------------------------------------------------------------------------- */
/* includes */

#include <kbdus.h>
#include <kbdus/inverter.h>
#include <kbdus/protocol.h>
#include <kbdus/protocols/ioctl.h>
#include <kbdus/utilities.h>

#include <linux/blkdev.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/uaccess.h>

/* -------------------------------------------------------------------------- */
/* request serialization and reply handling */

static int kbdusioctl_serialize_request_(
    const struct kbdus_inverter_request *p_req,
    struct kbdusioctl_request *request
    )
{
    void __user *buffer_usrptr;
    struct bio_vec bvec;
    struct req_iterator req_iter;
    void *bvec_mapped_page;
    int ret;

    buffer_usrptr = (void __user *)request->buffer_ptr;

    request->handle_seqnum = p_req->handle_seqnum;
    request->handle_index  = p_req->handle_index;
    request->type          = p_req->type;

    switch (p_req->type)
    {
    case KBDUS_REQUEST_TYPE_WRITE:
    case KBDUS_REQUEST_TYPE_WRITE_SAME:
    case KBDUS_REQUEST_TYPE_FUA_WRITE:

        if (!kbdus_access_ok(
            KBDUS_VERIFY_WRITE,
            buffer_usrptr,
            (size_t)blk_rq_bytes(p_req->queue_req)
            ))
        {
            return -EFAULT;
        }

        rq_for_each_segment(bvec, p_req->queue_req, req_iter)
        {
            bvec_mapped_page = kmap(bvec.bv_page);

            ret = __copy_to_user(
                buffer_usrptr,
                bvec_mapped_page + bvec.bv_offset,
                bvec.bv_len
                );

            kunmap(bvec_mapped_page);

            if (ret != 0)
                return -EFAULT;

            buffer_usrptr += bvec.bv_len;
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
            if (copy_to_user(
                buffer_usrptr,
                p_req->ioctl_req_argument_buffer,
                (size_t)_IOC_SIZE(p_req->ioctl_req_command)
                ) != 0)
            {
                return -EFAULT;
            }
        }

        break;
    }

    // success

    return 0;
}

static int kbdusioctl_handle_reply_(
    struct kbdus_inverter *inverter,
    const struct kbdusioctl_reply *reply
    )
{
    const struct kbdus_inverter_request *p_req;
    const void __user *buffer_usrptr;
    struct bio_vec bvec;
    struct req_iterator req_iter;
    void *bvec_mapped_page;
    int ret;

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
        buffer_usrptr = (void __user *)reply->buffer_ptr;

        switch (p_req->type)
        {
        case KBDUS_REQUEST_TYPE_READ:

            if (!kbdus_access_ok(
                KBDUS_VERIFY_READ,
                buffer_usrptr,
                (size_t)blk_rq_bytes(p_req->queue_req)
                ))
            {
                goto error_fault;
            }

            rq_for_each_segment(bvec, p_req->queue_req, req_iter)
            {
                bvec_mapped_page = kmap(bvec.bv_page);

                ret = __copy_from_user(
                    bvec_mapped_page + bvec.bv_offset,
                    buffer_usrptr,
                    bvec.bv_len
                    );

                kunmap(bvec_mapped_page);

                if (ret != 0)
                    goto error_fault;

                buffer_usrptr += bvec.bv_len;
            }

            break;

        case KBDUS_REQUEST_TYPE_IOCTL:

            if (_IOC_DIR(p_req->ioctl_req_command) & _IOC_WRITE)
            {
                if (copy_from_user(
                    p_req->ioctl_req_argument_buffer,
                    buffer_usrptr,
                    (size_t)_IOC_SIZE(p_req->ioctl_req_command)
                    ) != 0)
                {
                    goto error_fault;
                }
            }

            break;
        }
    }

    // commit request completion and succeed

    kbdus_inverter_commit_request_completion(
        inverter, p_req, (int)reply->negated_errno
        );

    return 0;

error_fault:
    kbdus_inverter_abort_request_completion(inverter, p_req);
    return -EFAULT;
}

/* -------------------------------------------------------------------------- */
/* protocol implementation */

static int kbdusioctl_receive_request_(
    struct kbdus_inverter *inverter,
    struct kbdusioctl_request __user *request_usrptr
    )
{
    struct kbdusioctl_request request;
    const struct kbdus_inverter_request *p_req;
    int ret;

    // validate user-space pointer

    if (!kbdus_access_ok(
        KBDUS_VERIFY_WRITE, request_usrptr, sizeof(*request_usrptr)
        ))
    {
        return -EFAULT;
    }

    // get payload buffer pointer from user space

    if (__get_user(request.buffer_ptr, &request_usrptr->buffer_ptr) != 0)
        return -EFAULT;

    // get request to be processed

    p_req = kbdus_inverter_begin_request_get(inverter);

    if (IS_ERR(p_req))
        return PTR_ERR(p_req);

    // put request into the request structure

    if ((ret = kbdusioctl_serialize_request_(p_req, &request)) != 0)
    {
        kbdus_inverter_abort_request_get(inverter, p_req);
        return ret;
    }

    // copy request request structure to user space

    if (__copy_to_user(request_usrptr, &request, sizeof(request)) != 0)
    {
        kbdus_inverter_abort_request_get(inverter, p_req);
        return -EFAULT;
    }

    // success

    kbdus_inverter_commit_request_get(inverter, p_req);
    return 0;
}

static int kbdusioctl_send_reply_(
    struct kbdus_inverter *inverter,
    const struct kbdusioctl_reply __user *reply_usrptr
    )
{
    struct kbdusioctl_reply reply;

    // get reply structure from user space

    if (copy_from_user(&reply, reply_usrptr, sizeof(reply)) != 0)
        return -EFAULT;

    // handle reply (if there is a reply at all)

    return kbdusioctl_handle_reply_(inverter, &reply);
}

static int kbdusioctl_send_reply_and_receive_request_(
    struct kbdus_inverter *inverter,
    union kbdusioctl_reply_and_request __user *rar_usrptr
    )
{
    union kbdusioctl_reply_and_request rar;
    int ret;
    const struct kbdus_inverter_request *p_req;

    // validate user-space pointer

    if (!kbdus_access_ok(KBDUS_VERIFY_WRITE, rar_usrptr, sizeof(*rar_usrptr)))
        return -EFAULT;

    // get reply structure from user space

    if (__copy_from_user(
        &rar.reply, &rar_usrptr->reply, sizeof(rar.reply)
        ) != 0)
    {
        return -EFAULT;
    }

    // handle reply (if there is a reply at all)

    if ((ret = kbdusioctl_handle_reply_(inverter, &rar.reply)) != 0)
        return ret;

    // get request to be processed

    p_req = kbdus_inverter_begin_request_get(inverter);

    if (IS_ERR(p_req))
        return PTR_ERR(p_req);

    // put request into the rar's request structure

    if ((ret = kbdusioctl_serialize_request_(p_req, &rar.request)) != 0)
    {
        kbdus_inverter_abort_request_get(inverter, p_req);
        return ret;
    }

    // copy request structure to user space

    if (__copy_to_user(
        &rar_usrptr->request, &rar.request, sizeof(rar.request)
        ) != 0)
    {
        kbdus_inverter_abort_request_get(inverter, p_req);
        return -EFAULT;
    }

    // success

    kbdus_inverter_commit_request_get(inverter, p_req);
    return 0;
}

// Implements ioctl commands for the control device.
static int kbdusioctl_handle_control_ioctl_(
    void *instance_data,
    struct kbdus_inverter *inverter,
    unsigned int command,
    void __user *argument_usrptr
    )
{
    switch (command)
    {
    case KBDUSIOCTL_IOCTL_RECEIVE_REQUEST:
        return kbdusioctl_receive_request_(inverter, argument_usrptr);

    case KBDUSIOCTL_IOCTL_SEND_REPLY:
        return kbdusioctl_send_reply_(inverter, argument_usrptr);

    case KBDUSIOCTL_IOCTL_SEND_REPLY_AND_RECEIVE_REQUEST:
        return kbdusioctl_send_reply_and_receive_request_(
            inverter, argument_usrptr
            );

    default:
        return -ENOTTY;
    }
}

/* -------------------------------------------------------------------------- */
/* subcomponent interface */

const struct kbdus_protocol kbdusioctl_protocol =
{
    .name                 = "ioctl",

    .handle_control_ioctl = kbdusioctl_handle_control_ioctl_,
};

/* -------------------------------------------------------------------------- */
