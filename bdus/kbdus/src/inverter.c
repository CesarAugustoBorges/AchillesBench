/* -------------------------------------------------------------------------- */
/* includes */

#include <kbdus/inverter.h>
#include <kbdus/utilities.h>

#include <linux/blk-mq.h>
#include <linux/blkdev.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/genhd.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/log2.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/timekeeping.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/wait.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0)
    #include <linux/sched/debug.h>
    #include <linux/sched/signal.h>
    #include <linux/sched/task.h>
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0)
    #include <linux/build_bug.h>
#else
    #include <linux/bug.h>
#endif

/* -------------------------------------------------------------------------- */
/* private -- constants */

enum
{
    KBDUS_INVERTER_FLAG_DEACTIVATED_               = 1u <<  0,
    KBDUS_INVERTER_FLAG_DEACTIVATED_NOT_FLUSHED_   = 1u <<  1,
    KBDUS_INVERTER_FLAG_TERMINATED_                = 1u <<  2,
    KBDUS_INVERTER_FLAG_SEND_DEVICE_AVAILABLE_     = 1u <<  3,

    KBDUS_INVERTER_FLAG_LIFO_REQUEST_TRANSMISSION_ = 1u <<  4,

    KBDUS_INVERTER_FLAG_SUPPORTS_READ_             = 1u <<  5,
    KBDUS_INVERTER_FLAG_SUPPORTS_WRITE_            = 1u <<  6,
    KBDUS_INVERTER_FLAG_SUPPORTS_FLUSH_            = 1u <<  7,

#if KBDUS_DEBUG

    KBDUS_INVERTER_FLAG_SUPPORTS_WRITE_SAME_       = 1u <<  8,
    KBDUS_INVERTER_FLAG_SUPPORTS_WRITE_ZEROS_      = 1u <<  9,
    KBDUS_INVERTER_FLAG_SUPPORTS_FUA_WRITE_        = 1u << 10,
    KBDUS_INVERTER_FLAG_SUPPORTS_DISCARD_          = 1u << 11,
    KBDUS_INVERTER_FLAG_SUPPORTS_SECURE_ERASE_     = 1u << 12,

#endif
};

enum
{
    // request wrapper is not holding a request
    KBDUS_REQ_STATE_FREE_,

    // request wrapper is awaiting to be gotten by
    // `kbdus_inverter_begin_request_get()`
    KBDUS_REQ_STATE_AWAITING_GET_,

    // request wrapper is being gotten, i.e.,
    // `kbdus_inverter_begin_request_get()` has been called but
    // `kbdus_inverter_commit_request_get()` or
    // `kbdus_inverter_abort_request_get()` have not
    KBDUS_REQ_STATE_BEING_GOTTEN_,

    // request structure has been gotten and is awaiting to be completed by
    // `kbdus_inverter_begin_request_completion()`
    KBDUS_REQ_STATE_AWAITING_COMPLETION_,

    // request wrapper is being completed, i.e.,
    // `kbdus_inverter_begin_request_completion()` has been called but
    // `kbdus_inverter_commit_request_completion()` or
    // `kbdus_inverter_abort_request_completion()` have not
    KBDUS_REQ_STATE_BEING_COMPLETED_,

    // request structure is holding an IOCTL request that has been fully
    // completed, i.e., `kbdus_inverter_commit_request_completion()` has been
    // called (queue requests transition directly from BEING_COMPLETED to FREE)
    KBDUS_REQ_STATE_IOCTL_COMPLETED_,

    KBDUS_NUM_REQ_STATES_,
};

static const struct kbdus_inverter_request kbdus_inverter_req_dev_available_ =
{
    .handle_index  = 0,
    .handle_seqnum = 0,
    .type          = KBDUS_REQUEST_TYPE_DEVICE_AVAILABLE,
};

static const struct kbdus_inverter_request kbdus_inverter_req_terminate_ =
{
    .handle_index  = 0,
    .handle_seqnum = 0,
    .type          = KBDUS_REQUEST_TYPE_TERMINATE,
};

static const struct kbdus_inverter_request kbdus_inverter_req_flush_terminate_ =
{
    .handle_index  = 0,
    .handle_seqnum = 0,
    .type          = KBDUS_REQUEST_TYPE_FLUSH_AND_TERMINATE,
};

/* -------------------------------------------------------------------------- */
/* private -- types */

struct kbdus_inverter_request_wrapper_
{
    // - 32-bit archs: bytes 0-7
    // - 64-bit archs: bytes 0-15
    struct list_head list;

    // - 32-bit archs: bytes 8-31
    // - 64-bit archs: bytes 16-39
    struct kbdus_inverter_request request;

    // - 32-bit archs: bytes 32-35
    // - 64-bit archs: bytes 40-43
    u32 state;

    // only valid for ioctl requests, used to store the result of completed
    // ioctl requests
    //
    // - 32-bit archs: bytes 36-39
    // - 64-bit archs: bytes 44-47
    int ioctl_req_negated_errno;

    // only valid for ioctl requests, only actually triggered when state becomes
    // "awaiting get", "awaiting completion", or "completed"
    //
    // - 32-bit archs: bytes 40-43
    // - 64-bit archs: bytes 48-55
    struct completion *ioctl_req_state_changed;

#if KBDUS_DEBUG

    // - 32-bit archs: bytes 44-51
    // - 64-bit archs: bytes 56-63
    u64 state_last_set_at;

    // pad to 64 bytes
    #if BITS_PER_LONG == 32
    u8 padding_[12];
    #endif

#else

    // pad to 64 bytes
    #if BITS_PER_LONG == 32
    u8 padding_[20];
    #else
    u8 padding_[8];
    #endif

#endif
};

struct kbdus_inverter
{
#if KBDUS_DEBUG

    u64 req_state_time_sum[KBDUS_NUM_REQ_STATES_];
    u64 req_state_time_count[KBDUS_NUM_REQ_STATES_];

#endif

    u32 flags;
    u32 num_reqs;

    struct list_head reqs_free;
    struct list_head reqs_awaiting_get;

    spinlock_t reqs_lock;
    struct completion req_is_awaiting_get;

    // note that only requests that warrant a reply (i.e., non-notification
    // requests) are put into a wrapper
    struct kbdus_inverter_request_wrapper_ *reqs_all;
};

/* -------------------------------------------------------------------------- */
/* private -- compatibility utilities */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,18,0)
    #define KBDUS_INVERTER_BLK_EH_DONE_ BLK_EH_DONE
#else
    #define KBDUS_INVERTER_BLK_EH_DONE_ BLK_EH_NOT_HANDLED
#endif

/* -------------------------------------------------------------------------- */
/* private -- functions */

static enum kbdus_request_type kbdus_inverter_queue_request_type_(
    const struct request *queue_req
    )
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,8,0)

    switch (req_op(queue_req))
    {
    case REQ_OP_READ:
        return KBDUS_REQUEST_TYPE_READ;

    case REQ_OP_WRITE:
        if (queue_req->cmd_flags & REQ_FUA)
            return KBDUS_REQUEST_TYPE_FUA_WRITE;
        else
            return KBDUS_REQUEST_TYPE_WRITE;

    case REQ_OP_WRITE_SAME:
        return KBDUS_REQUEST_TYPE_WRITE_SAME;

    #if LINUX_VERSION_CODE >= KERNEL_VERSION(4,12,0)
        case REQ_OP_WRITE_ZEROES:
            if (queue_req->cmd_flags & REQ_NOUNMAP)
                return KBDUS_REQUEST_TYPE_WRITE_ZEROS_NO_UNMAP;
            else
                return KBDUS_REQUEST_TYPE_WRITE_ZEROS_MAY_UNMAP;
    #elif LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0)
        case REQ_OP_WRITE_ZEROES:
            return KBDUS_REQUEST_TYPE_WRITE_ZEROS_NO_UNMAP;
    #endif

    case REQ_OP_FLUSH:
        return KBDUS_REQUEST_TYPE_FLUSH;

    case REQ_OP_DISCARD:
        return KBDUS_REQUEST_TYPE_DISCARD;

    case REQ_OP_SECURE_ERASE:
        return KBDUS_REQUEST_TYPE_SECURE_ERASE;

    default:
        kbdus_assert(false);
        return -1;
    }

#else

    if (queue_req->cmd_flags & (REQ_DISCARD | REQ_SECURE))
        return KBDUS_REQUEST_TYPE_SECURE_ERASE;

    else if (queue_req->cmd_flags & REQ_DISCARD)
        return KBDUS_REQUEST_TYPE_DISCARD;

    else if (queue_req->cmd_flags & REQ_FLUSH)
        return KBDUS_REQUEST_TYPE_FLUSH;

    else if (queue_req->cmd_flags & REQ_WRITE_SAME)
        return KBDUS_REQUEST_TYPE_WRITE_SAME;

    else if (queue_req->cmd_flags & (REQ_WRITE | REQ_FUA))
        return KBDUS_REQUEST_TYPE_FUA_WRITE;

    else if (queue_req->cmd_flags & REQ_WRITE)
        return KBDUS_REQUEST_TYPE_WRITE;

    else
        return KBDUS_REQUEST_TYPE_READ;

#endif
}

static bool kbdus_inverter_ensure_queue_req_is_supported_(
    const struct kbdus_inverter *inverter,
    enum kbdus_request_type req_type
    )
{
    switch (req_type)
    {
    // these two request types can always end up in the request queue

    case KBDUS_REQUEST_TYPE_READ:
        return inverter->flags & KBDUS_INVERTER_FLAG_SUPPORTS_READ_;

    case KBDUS_REQUEST_TYPE_WRITE:
        return inverter->flags & KBDUS_INVERTER_FLAG_SUPPORTS_WRITE_;

#if KBDUS_DEBUG

    // but the following should only appear if they are explicitly enabled

    case KBDUS_REQUEST_TYPE_WRITE_SAME:
        kbdus_assert(inverter->flags & KBDUS_INVERTER_FLAG_SUPPORTS_WRITE_SAME_);
        return inverter->flags & KBDUS_INVERTER_FLAG_SUPPORTS_WRITE_SAME_;

    case KBDUS_REQUEST_TYPE_WRITE_ZEROS_NO_UNMAP:
    case KBDUS_REQUEST_TYPE_WRITE_ZEROS_MAY_UNMAP:
        kbdus_assert(inverter->flags & KBDUS_INVERTER_FLAG_SUPPORTS_WRITE_ZEROS_);
        return inverter->flags & KBDUS_INVERTER_FLAG_SUPPORTS_WRITE_ZEROS_;

    case KBDUS_REQUEST_TYPE_FUA_WRITE:
        kbdus_assert(inverter->flags & KBDUS_INVERTER_FLAG_SUPPORTS_FUA_WRITE_);
        return inverter->flags & KBDUS_INVERTER_FLAG_SUPPORTS_FUA_WRITE_;

    case KBDUS_REQUEST_TYPE_FLUSH:
        kbdus_assert(inverter->flags & KBDUS_INVERTER_FLAG_SUPPORTS_FLUSH_);
        return inverter->flags & KBDUS_INVERTER_FLAG_SUPPORTS_FLUSH_;

    case KBDUS_REQUEST_TYPE_DISCARD:
        kbdus_assert(inverter->flags & KBDUS_INVERTER_FLAG_SUPPORTS_DISCARD_);
        return inverter->flags & KBDUS_INVERTER_FLAG_SUPPORTS_DISCARD_;

    case KBDUS_REQUEST_TYPE_SECURE_ERASE:
        kbdus_assert(inverter->flags & KBDUS_INVERTER_FLAG_SUPPORTS_SECURE_ERASE_);
        return inverter->flags & KBDUS_INVERTER_FLAG_SUPPORTS_SECURE_ERASE_;

    // and there aren't any more request types

    default:
        kbdus_assert(false);
        return false;

#else

    default:
        return true;

#endif
    }
}

static void kbdus_inverter_complete_queue_request_(
    struct request *queue_req,
    int negated_errno
    )
{
    // set errno in queue req's pdu

    *(int *)blk_mq_rq_to_pdu(queue_req) = negated_errno;

    #if LINUX_VERSION_CODE >= KERNEL_VERSION(4,12,0)
        blk_mq_complete_request(queue_req);
    #elif LINUX_VERSION_CODE >= KERNEL_VERSION(4,3,0)
        blk_mq_complete_request(queue_req, negated_errno);
    #else
        queue_req->errors = negated_errno;
        blk_mq_complete_request(queue_req);
    #endif
}

static struct kbdus_inverter_request_wrapper_ *
    kbdus_inverter_handle_index_to_wrapper_(
    struct kbdus_inverter *inverter,
    u16 index
    )
{
    index -= (u16)1;

    if ((u32)index >= inverter->num_reqs)
        return NULL;

    return &inverter->reqs_all[index];
}

/* -------------------------------------------------------------------------- */
/* private -- request management utilities */

// Must be called with `inverter->reqs_lock` held.
static void kbdus_inverter_wrapper_set_state_(
    struct kbdus_inverter *inverter,
    struct kbdus_inverter_request_wrapper_ *wrapper,
    u32 new_state
    )
{
#if KBDUS_DEBUG

    u64 cur_time;
    u64 req_dur;

    // get current time and compute request state duration

    cur_time = ktime_get_ns();
    req_dur = cur_time - wrapper->state_last_set_at;

    // update stats

    inverter->req_state_time_sum[wrapper->state] += req_dur;
    inverter->req_state_time_count[wrapper->state] += 1ull;

    // update time at which wrapper state was last set

    wrapper->state_last_set_at = cur_time;

#endif

    wrapper->state = new_state;
}

// Must be called with `inverter->reqs_lock` held.
static void kbdus_inverter_wrapper_to_awaiting_get_(
    struct kbdus_inverter *inverter,
    struct kbdus_inverter_request_wrapper_ *wrapper
    )
{
    kbdus_assert_if_debug(
        wrapper->state == KBDUS_REQ_STATE_FREE_ ||
        wrapper->state == KBDUS_REQ_STATE_AWAITING_COMPLETION_ ||
        wrapper->state == KBDUS_REQ_STATE_BEING_GOTTEN_
        );

    // put wrapper into "awaiting get" list

    if (wrapper->state == KBDUS_REQ_STATE_FREE_)
        list_move_tail(&wrapper->list, &inverter->reqs_awaiting_get);
    else
        list_add(&wrapper->list, &inverter->reqs_awaiting_get);

    // set wrapper state

    kbdus_inverter_wrapper_set_state_(
        inverter, wrapper, KBDUS_REQ_STATE_AWAITING_GET_
        );

    // notify single waiter that request is awaiting get

    complete(&inverter->req_is_awaiting_get);

    // notify waiter of ioctl request state change

    if (wrapper->request.type == KBDUS_REQUEST_TYPE_IOCTL)
        complete(wrapper->ioctl_req_state_changed);
}

// Must be called with `inverter->reqs_lock` held.
static void kbdus_inverter_wrapper_to_being_gotten_(
    struct kbdus_inverter *inverter,
    struct kbdus_inverter_request_wrapper_ *wrapper
    )
{
    kbdus_assert_if_debug(
        wrapper->state == KBDUS_REQ_STATE_AWAITING_GET_
        );

    // remove wrapper from "awaiting get" list

    list_del(&wrapper->list);

    // set wrapper state

    kbdus_inverter_wrapper_set_state_(
        inverter, wrapper, KBDUS_REQ_STATE_BEING_GOTTEN_
        );
}

// Must be called with `inverter->reqs_lock` held.
static void kbdus_inverter_wrapper_to_awaiting_completion_(
    struct kbdus_inverter *inverter,
    struct kbdus_inverter_request_wrapper_ *wrapper
    )
{
    kbdus_assert_if_debug(
        wrapper->state == KBDUS_REQ_STATE_BEING_GOTTEN_ ||
        wrapper->state == KBDUS_REQ_STATE_BEING_COMPLETED_
        );

    // set wrapper state

    kbdus_inverter_wrapper_set_state_(
        inverter, wrapper, KBDUS_REQ_STATE_AWAITING_COMPLETION_
        );

    // notify waiter of ioctl request state change

    if (wrapper->request.type == KBDUS_REQUEST_TYPE_IOCTL)
        complete(wrapper->ioctl_req_state_changed);
}

// Must be called with `inverter->reqs_lock` held.
static void kbdus_inverter_wrapper_to_being_completed_(
    struct kbdus_inverter *inverter,
    struct kbdus_inverter_request_wrapper_ *wrapper
    )
{
    kbdus_assert_if_debug(
        wrapper->state == KBDUS_REQ_STATE_AWAITING_COMPLETION_
        );

    // set wrapper state

    kbdus_inverter_wrapper_set_state_(
        inverter, wrapper, KBDUS_REQ_STATE_BEING_COMPLETED_
        );
}

// Must be called with `inverter->reqs_lock` held.
static void kbdus_inverter_wrapper_to_ioctl_completed_(
    struct kbdus_inverter *inverter,
    struct kbdus_inverter_request_wrapper_ *wrapper,
    int negated_errno
    )
{
    kbdus_assert(
        wrapper->state == KBDUS_REQ_STATE_AWAITING_GET_ ||
        wrapper->state == KBDUS_REQ_STATE_BEING_GOTTEN_ ||
        wrapper->state == KBDUS_REQ_STATE_AWAITING_COMPLETION_ ||
        wrapper->state == KBDUS_REQ_STATE_BEING_COMPLETED_
        );

    // increment wrapper seqnum

    wrapper->request.handle_seqnum += 1;

    // remove wrapper from "awaiting get" list

    if (wrapper->state == KBDUS_REQ_STATE_AWAITING_GET_)
        list_del(&wrapper->list);

    // set wrapper state

    kbdus_inverter_wrapper_set_state_(
        inverter, wrapper, KBDUS_REQ_STATE_IOCTL_COMPLETED_
        );

    // set ioctl negated errno

    wrapper->ioctl_req_negated_errno = negated_errno;

    // notify waiter of ioctl request state change

    complete(wrapper->ioctl_req_state_changed);
}

// Must be called with `inverter->reqs_lock` held.
static void kbdus_inverter_wrapper_to_free_(
    struct kbdus_inverter *inverter,
    struct kbdus_inverter_request_wrapper_ *wrapper,
    int queue_req_negated_errno
    )
{
    kbdus_assert_if_debug(
        wrapper->state == KBDUS_REQ_STATE_AWAITING_GET_ ||
        wrapper->state == KBDUS_REQ_STATE_BEING_GOTTEN_ ||
        wrapper->state == KBDUS_REQ_STATE_AWAITING_COMPLETION_ ||
        wrapper->state == KBDUS_REQ_STATE_BEING_COMPLETED_ ||
        wrapper->state == KBDUS_REQ_STATE_IOCTL_COMPLETED_
        );

    // complete queue request

    if (wrapper->request.type != KBDUS_REQUEST_TYPE_IOCTL)
    {
        kbdus_inverter_complete_queue_request_(
            wrapper->request.queue_req,
            queue_req_negated_errno
            );
    }

    // increment wrapper seqnum

    if (wrapper->state != KBDUS_REQ_STATE_IOCTL_COMPLETED_)
        wrapper->request.handle_seqnum += 1;

    // put wrapper into "awaiting get" list

    if (wrapper->state == KBDUS_REQ_STATE_AWAITING_GET_)
        list_move(&wrapper->list, &inverter->reqs_free);
    else
        list_add(&wrapper->list, &inverter->reqs_free);

    // set wrapper state

    kbdus_inverter_wrapper_set_state_(inverter, wrapper, KBDUS_REQ_STATE_FREE_);
}

// Must be called with `inverter->reqs_lock` held.
static void kbdus_inverter_wrapper_cancel_due_to_termination(
    struct kbdus_inverter *inverter,
    struct kbdus_inverter_request_wrapper_ *wrapper
    )
{
    if (wrapper->request.type == KBDUS_REQUEST_TYPE_IOCTL)
        kbdus_inverter_wrapper_to_ioctl_completed_(inverter, wrapper, -ENODEV);
    else
        kbdus_inverter_wrapper_to_free_(inverter, wrapper, -EIO);
}

static struct kbdus_inverter_request_wrapper_ *
    kbdus_inverter_handle_ioctl_submit_(
    struct kbdus_inverter *inverter,
    unsigned int command,
    void *argument_buffer,
    struct completion *state_changed
    )
{
    struct kbdus_inverter_request_wrapper_ *wrapper;

    // lock request lock

    spin_lock_irq(&inverter->reqs_lock);

    // perform some sanity checks

    kbdus_assert(!list_empty(&inverter->reqs_free));

    // check if inverter was terminated

    if (inverter->flags & KBDUS_INVERTER_FLAG_TERMINATED_)
    {
        spin_unlock_irq(&inverter->reqs_lock);
        return ERR_PTR(-ENODEV);
    }

    // get free request structure

    wrapper = list_first_entry(
        &inverter->reqs_free,
        struct kbdus_inverter_request_wrapper_,
        list
        );

    // initialize request

    wrapper->request.type                      = KBDUS_REQUEST_TYPE_IOCTL;
    wrapper->request.ioctl_req_command         = command;
    wrapper->request.ioctl_req_argument_buffer = argument_buffer;

    wrapper->ioctl_req_state_changed = state_changed;

    kbdus_inverter_wrapper_to_awaiting_get_(inverter, wrapper);

    // unlock request lock

    spin_unlock_irq(&inverter->reqs_lock);

    // return wrapper

    return wrapper;
}

// Must be called with `inverter->reqs_lock` held.
static void kbdus_inverter_handle_ioctl_cancel_(
    struct kbdus_inverter *inverter,
    struct kbdus_inverter_request_wrapper_ *wrapper
    )
{
    while (true)
    {
        switch (wrapper->state)
        {
        case KBDUS_REQ_STATE_BEING_GOTTEN_:
        case KBDUS_REQ_STATE_BEING_COMPLETED_:
            spin_unlock_irq(&inverter->reqs_lock);
            wait_for_completion(wrapper->ioctl_req_state_changed);
            spin_lock_irq(&inverter->reqs_lock);
            break;

        case KBDUS_REQ_STATE_AWAITING_GET_:
        case KBDUS_REQ_STATE_AWAITING_COMPLETION_:
            kbdus_inverter_wrapper_to_ioctl_completed_(inverter, wrapper, 0);
            return;

        case KBDUS_REQ_STATE_IOCTL_COMPLETED_:
            return;
        }
    }
}

// If interrupted, completes request before returning.
static int kbdus_inverter_handle_ioctl_wait_(
    struct kbdus_inverter *inverter,
    struct kbdus_inverter_request_wrapper_ *wrapper
    )
{
    int ret;

    while (true)
    {
        ret = wait_for_completion_interruptible(
            wrapper->ioctl_req_state_changed
            );

        spin_lock_irq(&inverter->reqs_lock);

        kbdus_assert(wrapper->state != KBDUS_REQ_STATE_FREE_);

        if (ret == -ERESTARTSYS)
        {
            // interrupted
            kbdus_inverter_handle_ioctl_cancel_(inverter, wrapper);
            break;
        }
        else if (wrapper->state == KBDUS_REQ_STATE_IOCTL_COMPLETED_)
        {
            // completed
            ret = wrapper->ioctl_req_negated_errno;
            break;
        }
        else
        {
            // not yet completed
            spin_unlock_irq(&inverter->reqs_lock);
        }
    }

    kbdus_inverter_wrapper_to_free_(inverter, wrapper, 0);

    spin_unlock_irq(&inverter->reqs_lock);

    return ret;
}

/* -------------------------------------------------------------------------- */
/* component init / exit */

int __init kbdus_inverter_init(void)
{
    BUILD_BUG_ON(sizeof(struct kbdus_inverter_request) != 24);
    BUILD_BUG_ON(sizeof(struct kbdus_inverter_request_wrapper_) != 64);

    return 0;
}

void kbdus_inverter_exit(void)
{
}

/* -------------------------------------------------------------------------- */
/* interface for request providers */

struct kbdus_inverter *kbdus_inverter_create(
    u32 max_active_reqs,
    const struct kbdus_device_config *device_config
    )
{
    struct kbdus_inverter *inverter;
    u32 i;

    // allocate inverter

    inverter = kmalloc(sizeof(*inverter), GFP_KERNEL);

    if (!inverter)
        return ERR_PTR(-ENOMEM);

    // allocate request wrappers

    // TODO note #1: We round up so that allocation is aligned, ultimately
    // aligning request wrappers with cache lines. Make this clearer and
    // probably expose this effect to kbdus clients to give them a change to
    // avoid wasting too much memory.

    // TODO note #2: Use kmalloc() with size rounded up if less than page, use
    // kmalloc_aligned() otherwise (with size rounded up to page?) to avoid
    // wasting too much memory due to round up to power of two.

    // TODO note #3: Can't allocate non-power-of-two number of pages. Just do
    // several page-sized allocations? Don't really need request wrappers to be
    // contiguous.

    inverter->reqs_all = kmalloc(
        sizeof(inverter->reqs_all[0])
            * roundup_pow_of_two((size_t)max_active_reqs),
        GFP_KERNEL
        );

    if (!inverter->reqs_all)
    {
        kfree(inverter);
        return ERR_PTR(-ENOMEM);
    }

    // initialize inverter structure

    inverter->flags = 0;

    if (device_config->lifo_request_transmission)
        inverter->flags |= KBDUS_INVERTER_FLAG_LIFO_REQUEST_TRANSMISSION_;

    if (device_config->supports_read)
        inverter->flags |= KBDUS_INVERTER_FLAG_SUPPORTS_READ_;

    if (device_config->supports_write)
        inverter->flags |= KBDUS_INVERTER_FLAG_SUPPORTS_WRITE_;

    if (device_config->supports_flush)
        inverter->flags |= KBDUS_INVERTER_FLAG_SUPPORTS_FLUSH_;

#if KBDUS_DEBUG

    if (device_config->supports_write_same)
        inverter->flags |= KBDUS_INVERTER_FLAG_SUPPORTS_WRITE_SAME_;

    if (device_config->supports_write_zeros)
        inverter->flags |= KBDUS_INVERTER_FLAG_SUPPORTS_WRITE_ZEROS_;

    if (device_config->supports_fua_write)
        inverter->flags |= KBDUS_INVERTER_FLAG_SUPPORTS_FUA_WRITE_;

    if (device_config->supports_discard)
        inverter->flags |= KBDUS_INVERTER_FLAG_SUPPORTS_DISCARD_;

    if (device_config->supports_secure_erase)
        inverter->flags |= KBDUS_INVERTER_FLAG_SUPPORTS_SECURE_ERASE_;

#endif

    inverter->num_reqs = max_active_reqs;

    INIT_LIST_HEAD(&inverter->reqs_free);
    INIT_LIST_HEAD(&inverter->reqs_awaiting_get);

    spin_lock_init(&inverter->reqs_lock);
    init_completion(&inverter->req_is_awaiting_get);

    for (i = 0; i < max_active_reqs; ++i)
    {
        inverter->reqs_all[i].state = KBDUS_REQ_STATE_FREE_;
        inverter->reqs_all[i].request.handle_index  = (u16)(i + 1);
        inverter->reqs_all[i].request.handle_seqnum = 0;

        list_add_tail(&inverter->reqs_all[i].list, &inverter->reqs_free);

#if KBDUS_DEBUG
        inverter->reqs_all[i].state_last_set_at = ktime_get_ns();
#endif
    }

#if KBDUS_DEBUG

    for (i = 0; i < KBDUS_NUM_REQ_STATES_; ++i)
    {
        inverter->req_state_time_sum[i] = 0ull;
        inverter->req_state_time_count[i] = 0ull;
    }

#endif

    // success

    return inverter;
}

void kbdus_inverter_destroy(struct kbdus_inverter *inverter)
{
    u32 i;

#if KBDUS_DEBUG

    u64 state_times[KBDUS_NUM_REQ_STATES_];

    for (i = 0; i < KBDUS_NUM_REQ_STATES_; ++i)
    {
        if (inverter->req_state_time_count[i] == 0)
        {
            state_times[i] = 0;
        }
        else
        {
            state_times[i] = (
                inverter->req_state_time_sum[i]
                / inverter->req_state_time_count[i]
                );
        }
    }

    kbdus_log_if_debug(
        "Inverter destroyed. ("
        "awaiting-get: %llu.%03llu us, "
        "being-gotten: %llu.%03llu us, "
        "awaiting-completion: %llu.%03llu us, "
        "being-completed: %llu.%03llu us, "
        "ioctl-completed: %llu.%03llu us"
        ")",
        state_times[KBDUS_REQ_STATE_AWAITING_GET_] / 1000ull,
        state_times[KBDUS_REQ_STATE_AWAITING_GET_] % 1000ull,
        state_times[KBDUS_REQ_STATE_BEING_GOTTEN_] / 1000ull,
        state_times[KBDUS_REQ_STATE_BEING_GOTTEN_] % 1000ull,
        state_times[KBDUS_REQ_STATE_AWAITING_COMPLETION_] / 1000ull,
        state_times[KBDUS_REQ_STATE_AWAITING_COMPLETION_] % 1000ull,
        state_times[KBDUS_REQ_STATE_BEING_COMPLETED_] / 1000ull,
        state_times[KBDUS_REQ_STATE_BEING_COMPLETED_] % 1000ull,
        state_times[KBDUS_REQ_STATE_IOCTL_COMPLETED_] / 1000ull,
        state_times[KBDUS_REQ_STATE_IOCTL_COMPLETED_] % 1000ull
        );

#endif

    // perform some sanity checks

    kbdus_assert(inverter->flags & KBDUS_INVERTER_FLAG_TERMINATED_);

    kbdus_assert(kbdus_list_length(&inverter->reqs_free) == inverter->num_reqs);
    kbdus_assert(list_empty(&inverter->reqs_awaiting_get));

    for (i = 0; i < inverter->num_reqs; ++i)
        kbdus_assert(inverter->reqs_all[i].state == KBDUS_REQ_STATE_FREE_);

    kbdus_assert(!spin_is_locked(&inverter->reqs_lock));

    // free inverter structure

    kfree(inverter->reqs_all);
    kfree(inverter);
}

void kbdus_inverter_terminate(struct kbdus_inverter *inverter)
{
    u32 i;
    struct kbdus_inverter_request_wrapper_ *wrapper;

    spin_lock_irq(&inverter->reqs_lock);

    if (!(inverter->flags & KBDUS_INVERTER_FLAG_TERMINATED_))
    {
        inverter->flags |= KBDUS_INVERTER_FLAG_TERMINATED_;

        /* fail requests awaiting get or awaiting completion */
        for (i = 0; i < inverter->num_reqs; ++i)
        {
            wrapper = &inverter->reqs_all[i];

            if (wrapper->state == KBDUS_REQ_STATE_AWAITING_GET_ ||
                wrapper->state == KBDUS_REQ_STATE_AWAITING_COMPLETION_)
            {
                kbdus_inverter_wrapper_cancel_due_to_termination(
                    inverter, wrapper
                    );
            }
        }

        /* notify waiters of infinite termination requests */
        complete_all(&inverter->req_is_awaiting_get);
    }

    spin_unlock_irq(&inverter->reqs_lock);
}

void kbdus_inverter_deactivate(struct kbdus_inverter *inverter, bool flush)
{
    spin_lock_irq(&inverter->reqs_lock);

    kbdus_assert(!(inverter->flags & KBDUS_INVERTER_FLAG_TERMINATED_));

    if (!(inverter->flags & KBDUS_INVERTER_FLAG_DEACTIVATED_))
    {
        inverter->flags |= KBDUS_INVERTER_FLAG_DEACTIVATED_;

        if (flush && (inverter->flags & KBDUS_INVERTER_FLAG_SUPPORTS_FLUSH_))
            inverter->flags |= KBDUS_INVERTER_FLAG_DEACTIVATED_NOT_FLUSHED_;
    }

    /* notify waiters of infinite termination requests */
    complete_all(&inverter->req_is_awaiting_get);

    spin_unlock_irq(&inverter->reqs_lock);
}

void kbdus_inverter_activate(struct kbdus_inverter *inverter)
{
    u32 i;
    struct kbdus_inverter_request_wrapper_ *wrapper;

    spin_lock_irq(&inverter->reqs_lock);

    kbdus_assert(!(inverter->flags & KBDUS_INVERTER_FLAG_TERMINATED_));

    // test and clear "deactivated" flag

    if (inverter->flags & KBDUS_INVERTER_FLAG_DEACTIVATED_)
    {
        inverter->flags &= ~(
            KBDUS_INVERTER_FLAG_DEACTIVATED_ |
            KBDUS_INVERTER_FLAG_DEACTIVATED_NOT_FLUSHED_
            );

        // reinitialize completion

        reinit_completion(&inverter->req_is_awaiting_get);

        // move all requests with state AWAITING_COMPLETION back to AWAITING_GET

        for (i = 0; i < inverter->num_reqs; ++i)
        {
            wrapper = &inverter->reqs_all[i];

            kbdus_assert(
                wrapper->state == KBDUS_REQ_STATE_FREE_ ||
                wrapper->state == KBDUS_REQ_STATE_AWAITING_GET_ ||
                wrapper->state == KBDUS_REQ_STATE_AWAITING_COMPLETION_ ||
                wrapper->state == KBDUS_REQ_STATE_IOCTL_COMPLETED_
                );

            switch (wrapper->state)
            {
            case KBDUS_REQ_STATE_AWAITING_GET_:
                complete(&inverter->req_is_awaiting_get);
                break;

            case KBDUS_REQ_STATE_AWAITING_COMPLETION_:
                kbdus_inverter_wrapper_to_awaiting_get_(inverter, wrapper);
                break;
            }
        }

        // notify waiters of "device available" request if appropriate

        if (inverter->flags & KBDUS_INVERTER_FLAG_SEND_DEVICE_AVAILABLE_)
            complete(&inverter->req_is_awaiting_get);
    }

    // unlock request lock

    spin_unlock_irq(&inverter->reqs_lock);
}

void kbdus_inverter_submit_device_available_request(
    struct kbdus_inverter *inverter
    )
{
    spin_lock_irq(&inverter->reqs_lock);

    if (!(inverter->flags & KBDUS_INVERTER_FLAG_SEND_DEVICE_AVAILABLE_))
    {
        inverter->flags |= KBDUS_INVERTER_FLAG_SEND_DEVICE_AVAILABLE_;
        complete(&inverter->req_is_awaiting_get);
    }

    spin_unlock_irq(&inverter->reqs_lock);
}

int kbdus_inverter_submit_queue_request(
    struct kbdus_inverter *inverter,
    struct request *queue_req,
    u16 *out_request_handle_index,
    u64 *out_request_handle_seqnum
    )
{
    enum kbdus_request_type req_type;
    unsigned long flags;
    struct kbdus_inverter_request_wrapper_ *wrapper;

    // get request type

    req_type = kbdus_inverter_queue_request_type_(queue_req);

    // lock request lock

    spin_lock_irqsave(&inverter->reqs_lock, flags);

    // perform some sanity checks

    kbdus_assert_if_debug(!list_empty(&inverter->reqs_free));

    // fail request if inverter was terminated

    if (inverter->flags & KBDUS_INVERTER_FLAG_TERMINATED_)
    {
        spin_unlock_irqrestore(&inverter->reqs_lock, flags);

        *out_request_handle_index  = 0;
        *out_request_handle_seqnum = 0;

        return -EIO;
    }

    // reject read or write if unsupported and perform some sanity checks

    if (!kbdus_inverter_ensure_queue_req_is_supported_(inverter, req_type))
    {
        spin_unlock_irqrestore(&inverter->reqs_lock, flags);

        *out_request_handle_index  = 0;
        *out_request_handle_seqnum = 0;

        return -EOPNOTSUPP;
    }

    // get free request wrapper

    wrapper = list_first_entry(
        &inverter->reqs_free,
        struct kbdus_inverter_request_wrapper_,
        list
        );

    // initialize request

    wrapper->request.type      = req_type;
    wrapper->request.queue_req = queue_req;

    // set request state and move wrapper to appropriate list

    kbdus_inverter_wrapper_to_awaiting_get_(inverter, wrapper);

    // start queue request

    blk_mq_start_request(queue_req);

    // unlock request lock

    spin_unlock_irqrestore(&inverter->reqs_lock, flags);

    // return request handle

    *out_request_handle_index  = wrapper->request.handle_index;
    *out_request_handle_seqnum = wrapper->request.handle_seqnum;

    // success

    return 0;
}

enum blk_eh_timer_return kbdus_inverter_timeout_queue_request(
    struct kbdus_inverter *inverter,
    u16 request_handle_index,
    u64 request_handle_seqnum
    )
{
    struct kbdus_inverter_request_wrapper_ *wrapper;
    unsigned long flags;
    int ret;

    // get wrapper

    wrapper = kbdus_inverter_handle_index_to_wrapper_(
        inverter,
        request_handle_index
        );

    // lock request lock

    spin_lock_irqsave(&inverter->reqs_lock, flags);

    // perform some sanity checks

    kbdus_assert(wrapper);

    kbdus_assert(
        wrapper->request.type == KBDUS_REQUEST_TYPE_READ ||
        wrapper->request.type == KBDUS_REQUEST_TYPE_WRITE ||
        wrapper->request.type == KBDUS_REQUEST_TYPE_WRITE_SAME ||
        wrapper->request.type == KBDUS_REQUEST_TYPE_WRITE_ZEROS_NO_UNMAP ||
        wrapper->request.type == KBDUS_REQUEST_TYPE_WRITE_ZEROS_MAY_UNMAP ||
        wrapper->request.type == KBDUS_REQUEST_TYPE_FUA_WRITE ||
        wrapper->request.type == KBDUS_REQUEST_TYPE_FLUSH ||
        wrapper->request.type == KBDUS_REQUEST_TYPE_DISCARD ||
        wrapper->request.type == KBDUS_REQUEST_TYPE_SECURE_ERASE
        );

    // ignore timeout if request handle seqnums don't match

    if (wrapper->request.handle_seqnum != request_handle_seqnum)
    {
        spin_unlock_irqrestore(&inverter->reqs_lock, flags);
        return KBDUS_INVERTER_BLK_EH_DONE_;
    }

    // check request state

    switch (wrapper->state)
    {
    case KBDUS_REQ_STATE_BEING_GOTTEN_:
    case KBDUS_REQ_STATE_BEING_COMPLETED_:

        // can't timeout requests in these states, cancel (and restart) timer

        ret = BLK_EH_RESET_TIMER;
        break;

    case KBDUS_REQ_STATE_AWAITING_GET_:
    case KBDUS_REQ_STATE_AWAITING_COMPLETION_:

        kbdus_inverter_wrapper_to_free_(inverter, wrapper, -ETIMEDOUT);

        ret = KBDUS_INVERTER_BLK_EH_DONE_;
        break;

    default:

        kbdus_assert(false);

        ret = KBDUS_INVERTER_BLK_EH_DONE_;
        break;
    }

    // unlock request lock

    spin_unlock_irqrestore(&inverter->reqs_lock, flags);

    // return result

    return ret;
}

int kbdus_inverter_submit_and_await_ioctl_request(
    struct kbdus_inverter *inverter,
    unsigned int command,
    void __user *argument_usrptr
    )
{
    size_t arg_size;
    void *arg_buffer;
    struct completion state_changed;
    struct kbdus_inverter_request_wrapper_ *wrapper;
    int ret;

    arg_size = (size_t)_IOC_SIZE(command);

    // allocate argument buffer and copy user space data to it (if applicable)

    if (_IOC_DIR(command) == _IOC_NONE)
    {
        arg_buffer = NULL;
    }
    else
    {
        if (!kbdus_access_ok(
            (_IOC_DIR(command) & _IOC_WRITE)
                ? KBDUS_VERIFY_WRITE
                : KBDUS_VERIFY_READ,
            argument_usrptr,
            arg_size
            ))
        {
            return -EFAULT;
        }

        // note: zero-out memory for security in case of kbdus bugs
        arg_buffer = kzalloc(arg_size, GFP_KERNEL);

        if (!arg_buffer)
            return -ENOMEM;

        if (_IOC_DIR(command) & _IOC_READ)
        {
            // read-only or read-write argument, copy data from caller

            if (__copy_from_user(arg_buffer, argument_usrptr, arg_size) != 0)
            {
                kfree(arg_buffer);
                return -EFAULT;
            }
        }
    }

    // submit

    init_completion(&state_changed);

    wrapper = kbdus_inverter_handle_ioctl_submit_(
        inverter, command, arg_buffer, &state_changed
        );

    if (IS_ERR(wrapper))
    {
        kfree(arg_buffer);
        return PTR_ERR(wrapper);
    }

    // wait

    ret = kbdus_inverter_handle_ioctl_wait_(inverter, wrapper);

    // copy argument buffer to user space and free it (if applicable)

    if (ret == 0 && (_IOC_DIR(command) & _IOC_WRITE))
    {
        // success and write-only or read-write argument, copy data to caller

        if (__copy_to_user(argument_usrptr, arg_buffer, arg_size) != 0)
            ret = -EFAULT;
    }

    kfree(arg_buffer);

    // return result

    return ret;
}

/* -------------------------------------------------------------------------- */
/* interface for request processors */

const struct kbdus_inverter_request *kbdus_inverter_begin_request_get(
    struct kbdus_inverter *inverter
    )
{
    int ret;
    struct kbdus_inverter_request_wrapper_ *wrapper;

    while (true)
    {
        // wait until a request is awaiting get (or until we are interrupted)

        if (inverter->flags & KBDUS_INVERTER_FLAG_LIFO_REQUEST_TRANSMISSION_)
        {
            ret = kbdus_wait_for_completion_interruptible_lifo(
                &inverter->req_is_awaiting_get
                );
        }
        else
        {
            ret = wait_for_completion_interruptible(
                &inverter->req_is_awaiting_get
                );
        }

        if (ret != 0)
            return ERR_PTR(-ERESTARTSYS);

        // lock request lock

        spin_lock_irq(&inverter->reqs_lock);

        // check if request is a notification request

        if (inverter->flags & KBDUS_INVERTER_FLAG_DEACTIVATED_NOT_FLUSHED_)
        {
            inverter->flags &= ~KBDUS_INVERTER_FLAG_DEACTIVATED_NOT_FLUSHED_;

            spin_unlock_irq(&inverter->reqs_lock);
            return &kbdus_inverter_req_flush_terminate_;
        }

        if (inverter->flags & (
            KBDUS_INVERTER_FLAG_DEACTIVATED_ | KBDUS_INVERTER_FLAG_TERMINATED_
            ))
        {
            spin_unlock_irq(&inverter->reqs_lock);
            return &kbdus_inverter_req_terminate_;
        }

        if (inverter->flags & KBDUS_INVERTER_FLAG_SEND_DEVICE_AVAILABLE_)
        {
            inverter->flags &= ~KBDUS_INVERTER_FLAG_SEND_DEVICE_AVAILABLE_;

            spin_unlock_irq(&inverter->reqs_lock);
            return &kbdus_inverter_req_dev_available_;
        }

        // ensure request awaiting get exists (might have been canceled in the
        // meantime)

        if (!list_empty(&inverter->reqs_awaiting_get))
            break;

        // unlock request lock

        spin_unlock_irq(&inverter->reqs_lock);
    }

    // get request wrapper

    wrapper = list_first_entry(
        &inverter->reqs_awaiting_get,
        struct kbdus_inverter_request_wrapper_,
        list
        );

    // advance request state

    kbdus_inverter_wrapper_to_being_gotten_(inverter, wrapper);

    // unlock request lock

    spin_unlock_irq(&inverter->reqs_lock);

    // return request

    return &wrapper->request;
}

void kbdus_inverter_commit_request_get(
    struct kbdus_inverter *inverter,
    const struct kbdus_inverter_request *request
    )
{
    struct kbdus_inverter_request_wrapper_ *wrapper;

    switch (request->type)
    {
    case KBDUS_REQUEST_TYPE_DEVICE_AVAILABLE:
    case KBDUS_REQUEST_TYPE_TERMINATE:
    case KBDUS_REQUEST_TYPE_FLUSH_AND_TERMINATE:
        break; // nothing to be done

    default:

        // get request wrapper

        wrapper = container_of(
            request,
            struct kbdus_inverter_request_wrapper_,
            request
            );

        // lock request lock

        spin_lock_irq(&inverter->reqs_lock);

        // perform some sanity checks

        kbdus_assert_if_debug(wrapper->state == KBDUS_REQ_STATE_BEING_GOTTEN_);

        // advance request state

        if (inverter->flags & KBDUS_INVERTER_FLAG_TERMINATED_)
            kbdus_inverter_wrapper_cancel_due_to_termination(inverter, wrapper);
        else
            kbdus_inverter_wrapper_to_awaiting_completion_(inverter, wrapper);

        // unlock request lock

        spin_unlock_irq(&inverter->reqs_lock);

        break;
    }
}

void kbdus_inverter_abort_request_get(
    struct kbdus_inverter *inverter,
    const struct kbdus_inverter_request *request
    )
{
    struct kbdus_inverter_request_wrapper_ *wrapper;

    switch (request->type)
    {
    case KBDUS_REQUEST_TYPE_DEVICE_AVAILABLE:

        // resubmit "device available" request

        kbdus_inverter_submit_device_available_request(inverter);

        break;

    case KBDUS_REQUEST_TYPE_TERMINATE:
        break; // nothing to be done

    case KBDUS_REQUEST_TYPE_FLUSH_AND_TERMINATE:
        inverter->flags |= KBDUS_INVERTER_FLAG_DEACTIVATED_NOT_FLUSHED_;
        break;

    default:

        // get request wrapper

        wrapper = container_of(
            request,
            struct kbdus_inverter_request_wrapper_,
            request
            );

        // lock request lock

        spin_lock_irq(&inverter->reqs_lock);

        // perform some sanity checks

        kbdus_assert_if_debug(wrapper->state == KBDUS_REQ_STATE_BEING_GOTTEN_);

        // advance request state

        if (inverter->flags & KBDUS_INVERTER_FLAG_TERMINATED_)
            kbdus_inverter_wrapper_cancel_due_to_termination(inverter, wrapper);
        else
            kbdus_inverter_wrapper_to_awaiting_get_(inverter, wrapper);

        // unlock request lock

        spin_unlock_irq(&inverter->reqs_lock);

        break;
    }
}

const struct kbdus_inverter_request *kbdus_inverter_begin_request_completion(
    struct kbdus_inverter *inverter,
    u16 request_handle_index,
    u64 request_handle_seqnum
    )
{
    struct kbdus_inverter_request_wrapper_ *wrapper;

    // get wrapper and ensure that handle index is valid

    wrapper = kbdus_inverter_handle_index_to_wrapper_(
        inverter,
        request_handle_index
        );

    if (!wrapper)
        return ERR_PTR(-EINVAL);

    // lock request lock

    spin_lock_irq(&inverter->reqs_lock);

    // ensure that handle seqnum matches

    if (wrapper->request.handle_seqnum != request_handle_seqnum)
    {
        spin_unlock_irq(&inverter->reqs_lock);
        return NULL;
    }

    // ensure that request is awaiting completion

    if (wrapper->state != KBDUS_REQ_STATE_AWAITING_COMPLETION_)
    {
        spin_unlock_irq(&inverter->reqs_lock);
        return ERR_PTR(-EINVAL);
    }

    // advance request state

    kbdus_inverter_wrapper_to_being_completed_(inverter, wrapper);

    // unlock request lock

    spin_unlock_irq(&inverter->reqs_lock);

    // return request

    return &wrapper->request;
}

void kbdus_inverter_commit_request_completion(
    struct kbdus_inverter *inverter,
    const struct kbdus_inverter_request *request,
    int negated_errno
    )
{
    struct kbdus_inverter_request_wrapper_ *wrapper;

    // get request wrapper

    wrapper = container_of(
        request,
        struct kbdus_inverter_request_wrapper_,
        request
        );

    // lock request lock

    spin_lock_irq(&inverter->reqs_lock);

    // perform some sanity checks

    kbdus_assert_if_debug(wrapper->state == KBDUS_REQ_STATE_BEING_COMPLETED_);

    kbdus_assert_if_debug(
        wrapper->request.type != KBDUS_REQUEST_TYPE_DEVICE_AVAILABLE &&
        wrapper->request.type != KBDUS_REQUEST_TYPE_TERMINATE &&
        wrapper->request.type != KBDUS_REQUEST_TYPE_FLUSH_AND_TERMINATE
        );

    // advance request state

    if (wrapper->request.type == KBDUS_REQUEST_TYPE_IOCTL)
    {
        // override error if inverter was terminated

        if (inverter->flags & KBDUS_INVERTER_FLAG_TERMINATED_)
            negated_errno = -ENODEV;

        // override positive or kernel-internal error values

        if (negated_errno > 0 ||
            negated_errno < -133 ||
            negated_errno == -ENOSYS)
        {
            negated_errno = -EIO;
        }

        kbdus_inverter_wrapper_to_ioctl_completed_(
            inverter, wrapper, negated_errno
            );
    }
    else
    {
        // override error if inverter was terminated

        if (inverter->flags & KBDUS_INVERTER_FLAG_TERMINATED_)
            negated_errno = -EIO;

        // allow only some error values that should be safe

        switch (negated_errno)
        {
        case 0:
        case -EBADE:
        case -EILSEQ:
        case -ENODATA:
        case -ENOLINK:
        case -ENOSPC:
        case -EREMOTEIO:
            break;

        default:
            negated_errno = -EIO;
            break;
        }

        kbdus_inverter_wrapper_to_free_(inverter, wrapper, negated_errno);
    }

    // unlock request lock

    spin_unlock_irq(&inverter->reqs_lock);
}

void kbdus_inverter_abort_request_completion(
    struct kbdus_inverter *inverter,
    const struct kbdus_inverter_request *request
    )
{
    struct kbdus_inverter_request_wrapper_ *wrapper;

    // get request wrapper

    wrapper = container_of(
        request,
        struct kbdus_inverter_request_wrapper_,
        request
        );

    // lock request lock

    spin_lock_irq(&inverter->reqs_lock);

    // perform some sanity checks

    kbdus_assert_if_debug(wrapper->state == KBDUS_REQ_STATE_BEING_COMPLETED_);

    kbdus_assert_if_debug(
        wrapper->request.type != KBDUS_REQUEST_TYPE_DEVICE_AVAILABLE &&
        wrapper->request.type != KBDUS_REQUEST_TYPE_TERMINATE &&
        wrapper->request.type != KBDUS_REQUEST_TYPE_FLUSH_AND_TERMINATE
        );

    // advance request state

    if (inverter->flags & KBDUS_INVERTER_FLAG_TERMINATED_)
        kbdus_inverter_wrapper_cancel_due_to_termination(inverter, wrapper);
    else
        kbdus_inverter_wrapper_to_awaiting_completion_(inverter, wrapper);

    // unlock request lock

    spin_unlock_irq(&inverter->reqs_lock);
}

/* -------------------------------------------------------------------------- */
