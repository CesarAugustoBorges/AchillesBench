/* -------------------------------------------------------------------------- */
/* includes */

#include <kbdus.h>
#include <kbdus/inverter.h>
#include <kbdus/protocol.h>
#include <kbdus/protocols/rw.h>
#include <kbdus/utilities.h>

#include <linux/blkdev.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/uio.h>

/* -------------------------------------------------------------------------- */
/* request serialization and reply deserialization */

static ssize_t kbdusrw_serialize_request_(
    struct iov_iter *to,
    const struct kbdus_inverter_request *req
    )
{
    struct kbdusrw_request_header request;
    size_t payload_bytes;
    struct bio_vec bvec;
    struct req_iterator req_iter;

    // fill in request header

    request.handle_seqnum = req->handle_seqnum;
    request.handle_index  = req->handle_index;
    request.type          = req->type;

    switch (req->type)
    {
    case KBDUS_REQUEST_TYPE_WRITE:
    case KBDUS_REQUEST_TYPE_WRITE_SAME:
    case KBDUS_REQUEST_TYPE_FUA_WRITE:
    case KBDUS_REQUEST_TYPE_READ:
    case KBDUS_REQUEST_TYPE_WRITE_ZEROS_NO_UNMAP:
    case KBDUS_REQUEST_TYPE_WRITE_ZEROS_MAY_UNMAP:
    case KBDUS_REQUEST_TYPE_DISCARD:
    case KBDUS_REQUEST_TYPE_SECURE_ERASE:

        request.arg64 = 512ull * (u64)blk_rq_pos(req->queue_req);
        request.arg32 = (u32)blk_rq_bytes(req->queue_req);

        break;

    case KBDUS_REQUEST_TYPE_IOCTL:

        request.arg32 = (u32)req->ioctl_req_command;

        break;
    }

    // copy request header to user space

    if (iov_iter_count(to) < sizeof(request))
        return -EINVAL;

    if (copy_to_iter(&request, sizeof(request), to) != sizeof(request))
        return -EFAULT;

    // copy data to user space if applicable

    payload_bytes = 0;

    switch (req->type)
    {
    case KBDUS_REQUEST_TYPE_WRITE:
    case KBDUS_REQUEST_TYPE_WRITE_SAME:
    case KBDUS_REQUEST_TYPE_FUA_WRITE:

        payload_bytes = (size_t)request.arg32;

        if (iov_iter_count(to) < payload_bytes)
            return -EINVAL;

        rq_for_each_segment(bvec, req->queue_req, req_iter)
        {
            if (copy_page_to_iter(
                bvec.bv_page,
                bvec.bv_offset,
                bvec.bv_len,
                to
                ) != bvec.bv_len)
            {
                return -EFAULT;
            }
        }

        break;

    case KBDUS_REQUEST_TYPE_IOCTL:

        if (_IOC_DIR(req->ioctl_req_command) & _IOC_READ)
        {
            payload_bytes = (size_t)_IOC_SIZE(req->ioctl_req_command);

            if (iov_iter_count(to) < payload_bytes)
                return -EINVAL;

            if (copy_to_iter(
                req->ioctl_req_argument_buffer,
                payload_bytes,
                to
                ) != payload_bytes)
            {
                return -EFAULT;
            }
        }

        break;
    }

    // return number of read (from user space's perspective) bytes

    return (ssize_t)(sizeof(struct kbdusrw_request_header) + payload_bytes);
}

static ssize_t kbdusrw_deserialize_reply_(
    struct iov_iter *from,
    const struct kbdusrw_reply_header *reply,
    const struct kbdus_inverter_request *req
    )
{
    size_t payload_bytes;
    struct bio_vec bvec;
    struct req_iterator req_iter;

    // copy data from user space if applicable and request succeeded

    payload_bytes = 0;

    if (reply->negated_errno == 0)
    {
        switch (req->type)
        {
        case KBDUS_REQUEST_TYPE_READ:

            payload_bytes = (size_t)blk_rq_bytes(req->queue_req);

            if (iov_iter_count(from) != payload_bytes)
                return -EINVAL;

            rq_for_each_segment(bvec, req->queue_req, req_iter)
            {
                if (copy_page_from_iter(
                    bvec.bv_page,
                    bvec.bv_offset,
                    bvec.bv_len,
                    from
                    ) != bvec.bv_len)
                {
                    return -EFAULT;
                }
            }

            break;

        case KBDUS_REQUEST_TYPE_IOCTL:

            if (_IOC_DIR(req->ioctl_req_command) & _IOC_WRITE)
            {
                payload_bytes = (size_t)_IOC_SIZE(req->ioctl_req_command);

                if (iov_iter_count(from) != payload_bytes)
                    return -EINVAL;

                if (copy_from_iter(
                    req->ioctl_req_argument_buffer,
                    payload_bytes,
                    from
                    ) != payload_bytes)
                {
                    return -EFAULT;
                }
            }
            else
            {
                if (iov_iter_count(from) != 0)
                    return -EINVAL;
            }

            break;

        default:

            if (iov_iter_count(from) != 0)
                return -EINVAL;

            break;
        }
    }

    // return number of written (from user space's perspective) bytes

    return (ssize_t)(sizeof(struct kbdusrw_reply_header) + payload_bytes);
}

/* -------------------------------------------------------------------------- */
/* protocol implementation */

static ssize_t kbdusrw_handle_control_read_iter_(
    void *instance_data,
    struct kbdus_inverter *inverter,
    struct iov_iter *to
    )
{
    const struct kbdus_inverter_request *request;
    ssize_t payload_bytes;

    // get request to be processed

    request = kbdus_inverter_begin_request_get(inverter);

    if (IS_ERR(request))
        return PTR_ERR(request);

    // serialize request

    payload_bytes = kbdusrw_serialize_request_(to, request);

    // commit / abort request get

    if (payload_bytes >= 0)
        kbdus_inverter_commit_request_get(inverter, request);
    else
        kbdus_inverter_abort_request_get(inverter, request);

    // return number of read (from user space's perspective) bytes

    return payload_bytes;
}

static ssize_t kbdusrw_handle_control_write_iter_(
    void *instance_data,
    struct kbdus_inverter *inverter,
    struct iov_iter *from
    )
{
    struct kbdusrw_reply_header reply;
    const struct kbdus_inverter_request *req;
    ssize_t payload_bytes;

    // copy reply header from user space

    if (iov_iter_count(from) < sizeof(reply))
        return -EINVAL;

    if (copy_from_iter(&reply, sizeof(reply), from) != sizeof(reply))
        return -EFAULT;

    // check if there is a reply at all

    if (reply.handle_index == 0)
        return 0;

    // get request from handle

    req = kbdus_inverter_begin_request_completion(
        inverter, reply.handle_index, reply.handle_seqnum
        );

    if (!req)
        return 0; // request timed out, was canceled, or was already completed

    if (IS_ERR(req))
        return PTR_ERR(req);

    // delegate remaining work

    payload_bytes = kbdusrw_deserialize_reply_(from, &reply, req);

    // commit / abort request get

    if (payload_bytes >= 0)
    {
        kbdus_inverter_commit_request_completion(
            inverter, req, (int)reply.negated_errno
            );
    }
    else
    {
        kbdus_inverter_abort_request_completion(inverter, req);
    }

    // return number of written (from user space's perspective) bytes

    return payload_bytes;
}

/* -------------------------------------------------------------------------- */
/* subcomponent interface */

const struct kbdus_protocol kbdusrw_protocol =
{
    .name                      = "rw",

    .handle_control_read_iter  = kbdusrw_handle_control_read_iter_,
    .handle_control_write_iter = kbdusrw_handle_control_write_iter_,
};

/* -------------------------------------------------------------------------- */
