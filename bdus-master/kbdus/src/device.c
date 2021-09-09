/* -------------------------------------------------------------------------- */
/* includes */

#include <kbdus.h>
#include <kbdus/config.h>
#include <kbdus/device.h>
#include <kbdus/inverter.h>
#include <kbdus/protocol.h>
#include <kbdus/utilities.h>

#include <linux/atomic.h>
#include <linux/blk-mq.h>
#include <linux/blkdev.h>
#include <linux/completion.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/genhd.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0)
    #include <linux/sched/task.h>
#endif

/* -------------------------------------------------------------------------- */
/* private -- types */

struct kbdus_device
{
    // Starts at 1, decremented by kbdus_device_destroy(), incremented by open()
    // on the device, decremented by release() on the device. Whoever decrements
    // it to 0 cleans it up. Used to avoid freeing the device structure after it
    // is destroyed but the device is still open and can receive ioctl calls.
    u32 ref_count;

    atomic_t state;

    struct kbdus_device_config config;

    struct kbdus_inverter *inverter;

    // request queue

    struct request_queue *queue;
    struct blk_mq_tag_set tag_set;

    // disk

    struct gendisk *disk;
    struct task_struct *add_disk_task;

    // ioctl requests

    int max_active_ioctl_reqs;
    struct semaphore ioctl_semaphore;
};

/* -------------------------------------------------------------------------- */
/* private -- variables */

// The major number for BDUS block devices.
static int kbdus_device_major_;

/* -------------------------------------------------------------------------- */
/* private -- kernel compatibility */

// Compatibility helpers to deal with kernel versions that use errno values
// where block status codes are now used.

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0)
    #define kbdus_device_blk_status_t_ blk_status_t
    #define kbdus_device_errno_to_blk_status_(error) errno_to_blk_status(error)
#else
    #define kbdus_device_blk_status_t_ int
    #define kbdus_device_errno_to_blk_status_(error) (error)
#endif

/* -------------------------------------------------------------------------- */
/* private -- utilities */

static bool kbdus_device_is_read_only_(const struct kbdus_device_config *config)
{
    return
        !config->supports_write        &&
        !config->supports_write_same   &&
        !config->supports_write_zeros  &&
        !config->supports_fua_write    &&
        !config->supports_discard      &&
        !config->supports_secure_erase;
}

// Validates the command and argument of an ioctl call.
static bool kbdus_device_is_valid_ioctl_(
    unsigned int command,
    void __user *argument_usrptr
    )
{
    size_t size;

    // validate command and argument and set argument pointer

    size = (size_t)_IOC_SIZE(command);

    switch (_IOC_DIR(command))
    {
    case _IOC_NONE:

        if (size != 0)
            return false; // invalid size

        if (argument_usrptr != NULL)
            return false; // invalid argument

        return true; // valid

    case _IOC_READ:
    case _IOC_WRITE:
    case _IOC_READ | _IOC_WRITE:

        if (size == 0 || size >= (1ul << 14))
            return false; // invalid size

        if (argument_usrptr == NULL)
            return false; // invalid argument

        return true; // valid

    default:
        return false; // invalid direction
    }
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0)
    #define kbdus_device_disk_to_queue_lock_(disk) (&(disk)->queue->queue_lock)
#else
    #define kbdus_device_disk_to_queue_lock_(disk) ((disk)->queue->queue_lock)
#endif

static struct kbdus_device *kbdus_device_get_ref_(struct gendisk *disk)
{
    struct kbdus_device *device;

    kbdus_assert(disk);
    kbdus_assert(disk->queue);

    spin_lock(kbdus_device_disk_to_queue_lock_(disk));

    if (disk->queue->queuedata)
    {
        device = disk->queue->queuedata;

        kbdus_assert(device->ref_count > 0);

        device->ref_count += 1;
    }
    else
    {
        device = NULL;
    }

    spin_unlock(kbdus_device_disk_to_queue_lock_(disk));

    return device;
}

static void kbdus_device_put_ref_(struct gendisk *disk)
{
    struct kbdus_device *device;

    kbdus_assert(disk);
    kbdus_assert(disk->queue);

    spin_lock(kbdus_device_disk_to_queue_lock_(disk));

    device = disk->queue->queuedata;

    kbdus_assert(device->ref_count > 0);

    device->ref_count -= 1;

    if (device->ref_count == 0)
    {
        kfree(device);
        disk->queue->queuedata = NULL;
    }

    spin_unlock(kbdus_device_disk_to_queue_lock_(disk));
}

/* -------------------------------------------------------------------------- */
/* private -- configuration validation and adjustment */

// Validate a device configuration. Returns the protocol whose name was in the
// `protocol_name` field of the configuration structure.
static bool kbdus_device_validate_config_(
    const struct kbdus_device_config *config
    )
{
    bool valid;

    valid = true;

    // operations -- supports_fua_write implies supports_flush

    valid = valid && (
        !config->supports_fua_write || config->supports_flush
        );

    // attributes -- logical_block_size

    valid = valid && (
        kbdus_is_power_of_two(config->logical_block_size) &&
        config->logical_block_size >= 512u &&
        config->logical_block_size <= (u32)PAGE_SIZE
        );

    // attributes -- physical_block_size

    valid = valid && (
        config->physical_block_size == 0 || (
            kbdus_is_power_of_two(config->physical_block_size) &&
            config->physical_block_size >= config->logical_block_size &&
            config->physical_block_size <= (u32)PAGE_SIZE
            )
        );

    // attributes -- size

    valid = valid && (
        kbdus_is_positive_multiple_of(
            config->size,
            max(config->physical_block_size, config->logical_block_size)
            )
        );

    // attributes -- max_read_write_size

    valid = valid && (
        config->max_read_write_size == 0 ||
        config->max_read_write_size >= (u32)PAGE_SIZE
        );

    // attributes -- max_write_same_size

    valid = valid && (
        config->max_write_same_size == 0 ||
        config->max_write_same_size >= config->logical_block_size
        );

    // attributes -- max_write_zeros_size

    valid = valid && (
        config->max_write_zeros_size == 0 ||
        config->max_write_zeros_size >= config->logical_block_size
        );

    // attributes -- max_discard_erase_size

    valid = valid && (
        config->max_discard_erase_size == 0 ||
        config->max_discard_erase_size >= config->logical_block_size
        );

    // attributes -- max_active_queue_reqs

    valid = valid && (
        config->max_active_queue_reqs >= 1 &&
        config->max_active_queue_reqs <= KBDUS_HARD_MAX_ACTIVE_QUEUE_REQS
        );

    // attributes -- max_active_ioctl_reqs

    valid = valid && (
        config->max_active_ioctl_reqs >= 1 &&
        config->max_active_ioctl_reqs <= KBDUS_HARD_MAX_ACTIVE_IOCTL_REQS
        );

    // return result

    return valid;
}

// Adjust a *previously validated* device configuration, setting the `major`
// field of the configuration and ensuring that 0 values are replaced by the
// appropriate defaults where applicable.
static void kbdus_device_adjust_config_(struct kbdus_device_config *config)
{
    // major

    config->major = (u32)kbdus_device_major_;

    // physical_block_size

    if (config->physical_block_size == 0)
        config->physical_block_size = config->logical_block_size;

    // max_read_write_size

    if (!config->supports_read &&
        !config->supports_write &&
        !config->supports_fua_write)
    {
        config->max_read_write_size = 0;
    }
    else
    {
        config->max_read_write_size = round_down(
            min_not_zero(config->max_read_write_size, 512u * 1024u),
            config->logical_block_size
            );
    }

    // max_write_same_size

    if (!config->supports_write_same)
    {
        config->max_write_same_size = 0;
    }
    else
    {
        config->max_write_same_size = round_down(
            min_not_zero(config->max_write_same_size, U32_MAX),
            config->logical_block_size
            );
    }

    // max_write_zeros_size

    if (!config->supports_write_zeros)
    {
        config->max_write_zeros_size = 0;
    }
    else
    {
        config->max_write_zeros_size = round_down(
            min_not_zero(config->max_write_zeros_size, U32_MAX),
            config->logical_block_size
            );
    }

    // max_discard_erase_size

    if (!config->supports_discard && !config->supports_secure_erase)
    {
        config->max_discard_erase_size = 0;
    }
    else
    {
        config->max_discard_erase_size = round_down(
            min_not_zero(config->max_discard_erase_size, U32_MAX),
            config->logical_block_size
            );
    }
}

/* -------------------------------------------------------------------------- */
/* private -- utility functions -- blk-mq setup */

// Atomically set or clear a request queue flag.
static void kbdus_device_queue_flag_(
    struct request_queue *queue, unsigned int flag, bool value
    )
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)

    if (value)
        blk_queue_flag_set(flag, queue);
    else
        blk_queue_flag_clear(flag, queue);

#else

    spin_lock(queue->queue_lock);

    if (value)
        queue_flag_set(flag, queue);
    else
        queue_flag_clear(flag, queue);

    spin_unlock(queue->queue_lock);

#endif
}

static struct request_queue *kbdus_device_create_queue_(
    struct blk_mq_tag_set *tag_set,
    const struct blk_mq_ops *ops,
    unsigned int queue_depth,
    unsigned int tag_set_flags,
    unsigned int cmd_size
    )
{
    // this function is similar to blk_mq_init_sq_queue(), which exists only
    // since Linux 4.19.0

    int ret;
    struct request_queue *queue;

    // initialize tag set

    memset(tag_set, 0, sizeof(*tag_set));

    tag_set->nr_hw_queues = 1;
    tag_set->queue_depth  = queue_depth;
    tag_set->numa_node    = NUMA_NO_NODE;
    tag_set->cmd_size     = cmd_size;
    tag_set->flags        = tag_set_flags;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0)
    tag_set->ops = ops;
#else
    tag_set->ops = (struct blk_mq_ops *)ops;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0)
    tag_set->nr_maps = 1;
#endif

    // allocate tag set

    ret = blk_mq_alloc_tag_set(tag_set);

    if (ret != 0)
        return ERR_PTR(ret);

    // create queue

    queue = blk_mq_init_queue(tag_set);

    if (IS_ERR(queue))
        blk_mq_free_tag_set(tag_set);

    // return queue (or error)

    return queue;
}

static void kbdus_device_config_request_queue_flags_(
    struct request_queue *queue,
    const struct kbdus_device_config *config
    )
{
    kbdus_device_queue_flag_(queue, QUEUE_FLAG_NONROT, !config->is_rotational);
    kbdus_device_queue_flag_(queue, QUEUE_FLAG_ADD_RANDOM, false);

    // discard requests

    kbdus_device_queue_flag_(
        queue,
        QUEUE_FLAG_DISCARD,
        (bool)config->supports_discard
        );

    // secure erase requests

    kbdus_device_queue_flag_(
        queue,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,8,0)
        QUEUE_FLAG_SECERASE,
#else
        QUEUE_FLAG_SECDISCARD,
#endif
        (bool)config->supports_secure_erase
        );

    // flush / fua_write requests

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,7,0)
    blk_queue_write_cache(
        queue,
        (bool)config->supports_flush,
        (bool)config->supports_fua_write
        );
#else
    blk_queue_flush(
        queue,
        (config->supports_flush ? REQ_FLUSH : 0)
        | (config->supports_fua_write ? REQ_FUA : 0)
        );
#endif
}

static void kbdus_device_config_request_queue_limits_(
    struct request_queue *queue,
    const struct kbdus_device_config *config
    )
{
    // logical / physical block sizes

    blk_queue_logical_block_size(
        queue, (unsigned short)config->logical_block_size
        );

    blk_queue_physical_block_size(
        queue, (unsigned int)config->physical_block_size
        );

    // read / write / fua write requests

    if (config->supports_read ||
        config->supports_write ||
        config->supports_fua_write)
    {
        blk_queue_max_hw_sectors(
            queue,
            (unsigned int)(config->max_read_write_size / 512u)
            );
    }
    else
    {
        // read, write, and fua_write not supported, but max_hw_sectors must be
        // >= PAGE_SIZE / 512

        blk_queue_max_hw_sectors(queue, (unsigned int)PAGE_SIZE / 512u);
    }

    // write same requests

    if (config->supports_write_same)
    {
        blk_queue_max_write_same_sectors(
            queue,
            (unsigned int)(config->max_write_same_size / 512u)
            );
    }

    // write zeros requests

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0)
    if (config->supports_write_zeros)
    {
        blk_queue_max_write_zeroes_sectors(
            queue,
            (unsigned int)(config->max_write_zeros_size / 512u)
            );
    }
#endif

    // discard / secure erase requests

    if (config->supports_discard || config->supports_secure_erase)
    {
        blk_queue_max_discard_sectors(
            queue,
            (unsigned int)(config->max_discard_erase_size / 512u)
            );

        queue->limits.discard_granularity =
            (unsigned int)config->logical_block_size;
    }
}

static int kbdus_device_add_disk_(void *argument)
{
    struct kbdus_device *device;
    struct completion *add_disk_task_started;

    device = ((void **)argument)[0];
    add_disk_task_started = ((void **)argument)[1];

    // inform waiters that task started

    complete(add_disk_task_started);

    // add disk

    add_disk(device->disk);

    // notify protocol

    kbdus_inverter_submit_device_available_request(device->inverter);

    // update device state

    atomic_cmpxchg(
        &device->state,
        KBDUS_DEVICE_STATE_UNAVAILABLE,
        KBDUS_DEVICE_STATE_ACTIVE
        );

    // success

    return 0;
}

/* -------------------------------------------------------------------------- */
/* private -- utility functions -- blk-mq request processing */

static int *kbdus_device_queue_req_errno_(struct request *req)
{
    return (int *)blk_mq_rq_to_pdu(req);
}

static u16 *kbdus_device_queue_req_handle_index_(struct request *req)
{
    return (u16 *)((char *)blk_mq_rq_to_pdu(req) + 4);
}

static u64 *kbdus_device_queue_req_handle_seqnum_(struct request *req)
{
    return (u64 *)((char *)blk_mq_rq_to_pdu(req) + 8);
}

static kbdus_device_blk_status_t_ kbdus_device_mq_ops_queue_rq_(
    struct blk_mq_hw_ctx *hctx,
    const struct blk_mq_queue_data *bd
    )
{
    struct kbdus_device *device;
    int ret;

    device = hctx->queue->queuedata;

    // delegate processing to the protocol

    ret = kbdus_inverter_submit_queue_request(
        device->inverter,
        bd->rq,
        kbdus_device_queue_req_handle_index_(bd->rq),
        kbdus_device_queue_req_handle_seqnum_(bd->rq)
        );

    // return result

    return kbdus_device_errno_to_blk_status_(ret);
}

static enum blk_eh_timer_return kbdus_device_mq_ops_timeout_(
    struct request *req,
    bool reserved
    )
{
    struct kbdus_device *device;

    device = req->q->queuedata;

    return kbdus_inverter_timeout_queue_request(
        device->inverter,
        *kbdus_device_queue_req_handle_index_(req),
        *kbdus_device_queue_req_handle_seqnum_(req)
        );
}

static void kbdus_device_mq_ops_complete_(struct request *req)
{
    int negated_errno;

    // end the request with the status stored in its PDU

    negated_errno = *kbdus_device_queue_req_errno_(req);

    blk_mq_end_request(req, kbdus_device_errno_to_blk_status_(negated_errno));
}

// Request queue operations.
static const struct blk_mq_ops kbdus_device_queue_ops_ =
{
    .queue_rq = kbdus_device_mq_ops_queue_rq_,
    .timeout  = kbdus_device_mq_ops_timeout_,
    .complete = kbdus_device_mq_ops_complete_,

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,9,0)
    .map_queues = NULL,
#else
    .map_queue  = blk_mq_map_queue,
#endif
};

/* -------------------------------------------------------------------------- */
/* private -- file operations */

// Implements the `ioctl` and `compat_ioctl` file operations for devices.
static int kbdus_device_ioctl_(
    struct block_device *bdev, fmode_t mode,
    unsigned int command, unsigned long argument
    )
{
    void __user *argument_usrptr;
    struct kbdus_device *device;
    int ret;

    argument_usrptr = (void __user *)argument;

    // perform some sanity checks

    kbdus_assert(bdev->bd_disk);

    // the kernel lets the driver handle BLKFLSBUF and BLKROSET if it can, but
    // we don't and just let the kernel do what it is supposed to do

    if (command == BLKFLSBUF || command == BLKROSET)
        return -ENOTTY;

    // ensure that device structure still exists and get reference to it
    // (`bdev->bd_disk` is always valid inside this function)

    device = kbdus_device_get_ref_(bdev->bd_disk);

    if (!device)
        return -ENODEV; // device was already destroyed

    // check if device supports ioctl

    if (device->max_active_ioctl_reqs == 0)
    {
        ret = -ENOTTY;
        goto out_put_ref;
    }

    // validate command and get user-space pointer to argument

    if (!kbdus_device_is_valid_ioctl_(command, argument_usrptr))
    {
        ret = -ENOTTY;
        goto out_put_ref;
    }

    // acquire semaphore to limit number of pending ioctls on protocol

    if (down_interruptible(&device->ioctl_semaphore) != 0)
    {
        ret = -ERESTARTSYS;
        goto out_put_ref;
    }

    // check if inverter still exists

    if (device->inverter)
    {
        // delegate behavior to inverter

        ret = kbdus_inverter_submit_and_await_ioctl_request(
            device->inverter,
            command,
            argument_usrptr
            );
    }
    else
    {
        // protocol instance does not exist anymore

        ret = -ENODEV;
    }

    // release semaphore

    up(&device->ioctl_semaphore);

out_put_ref:

    // free device structure if holding last reference (`bdev->bd_disk` is
    // always valid inside this function)

    kbdus_device_put_ref_(bdev->bd_disk);

    // return result

    return ret;
}

// File operations for devices.
static const struct block_device_operations kbdus_device_ops_ =
{
    .owner        = THIS_MODULE,

    .ioctl        = kbdus_device_ioctl_,
    .compat_ioctl = kbdus_device_ioctl_,
};

/* -------------------------------------------------------------------------- */
/* component init / exit */

int __init kbdus_device_init(void)
{
    // register block device

    kbdus_device_major_ = register_blkdev(0, "bdus");

    if (kbdus_device_major_ < 0)
        return kbdus_device_major_;

    // success

    return 0;
}

void kbdus_device_exit(void)
{
    unregister_blkdev(kbdus_device_major_, "bdus");
}

/* -------------------------------------------------------------------------- */
/* component interface */

int kbdus_device_validate_and_adjust_config(struct kbdus_device_config *config)
{
    // validate configuration

    if (!kbdus_device_validate_config_(config))
        return -EINVAL;

    // adjust configuration

    kbdus_device_adjust_config_(config);

    // success

    return 0;
}

struct kbdus_device *kbdus_device_create(
    const struct kbdus_device_config *config
    )
{
    struct kbdus_device *device;
    int ret_error;
    struct completion add_disk_task_started;
    void *add_disk_task_argument[2];

    // allocate and initialize device structure a bit

    device = kmalloc(sizeof(*device), GFP_KERNEL);

    if (!device)
    {
        ret_error = -ENOMEM;
        goto error;
    }

    atomic_set(&device->state, KBDUS_DEVICE_STATE_UNAVAILABLE);

    device->config = *config;

    device->ref_count = 1;

    if (config->supports_ioctl)
    {
        device->max_active_ioctl_reqs = (int)config->max_active_ioctl_reqs;
        sema_init(&device->ioctl_semaphore, (int)config->max_active_ioctl_reqs);
    }
    else
    {
        device->max_active_ioctl_reqs = 0;
    }

    // create inverter

    device->inverter = kbdus_inverter_create(
        config->max_active_queue_reqs + (u32)device->max_active_ioctl_reqs,
        config
        );

    if (IS_ERR(device->inverter))
    {
        ret_error = PTR_ERR(device->inverter);
        goto error_free_device;
    }

    // initialize request queue

    device->queue = kbdus_device_create_queue_(
        &device->tag_set,
        &kbdus_device_queue_ops_,
        (unsigned int)config->max_active_queue_reqs,
        config->should_merge_requests ? BLK_MQ_F_SHOULD_MERGE : 0,
        16u
        );

    if (IS_ERR(device->queue))
    {
        ret_error = PTR_ERR(device->queue);
        goto error_destroy_inverter;
    }

    device->queue->queuedata = device;

    kbdus_device_config_request_queue_flags_(device->queue, config);
    kbdus_device_config_request_queue_limits_(device->queue, config);

    // initialize task for adding the disk

    init_completion(&add_disk_task_started);

    add_disk_task_argument[0] = device;
    add_disk_task_argument[1] = &add_disk_task_started;

    device->add_disk_task = kthread_create(
        kbdus_device_add_disk_,
        add_disk_task_argument,
        "kbdus_device_add_disk_"
        );

    if (IS_ERR(device->add_disk_task))
    {
        ret_error = PTR_ERR(device->add_disk_task);
        goto error_cleanup_queue;
    }

    // initialize gendisk

    device->disk = alloc_disk(
        config->disable_partition_scanning ? 1 : DISK_MAX_PARTS
        );

    if (!device->disk)
    {
        ret_error = -EIO;
        goto error_stop_add_disk_task;
    }

    if (config->disable_partition_scanning)
        device->disk->flags |= GENHD_FL_NO_PART_SCAN;

    device->disk->major       = kbdus_device_major_;
    device->disk->first_minor = config->first_minor;
    device->disk->fops        = &kbdus_device_ops_;
    device->disk->queue       = device->queue;

    ret_error = snprintf(
        device->disk->disk_name, DISK_NAME_LEN,
        "bdus-%u", config->index
        );

    kbdus_assert(ret_error < DISK_NAME_LEN);

    set_capacity(device->disk, (sector_t)(config->size / 512ull));
    set_disk_ro(device->disk, (int)kbdus_device_is_read_only_(config));

    // "get" `add_disk_task` so that it is still around when
    // `kbdus_device_destroy()` and thus `kthread_stop()` is called, even if the
    // task already ended

    get_task_struct(device->add_disk_task);

    // run add_disk_task

    wake_up_process(device->add_disk_task);

    // ensure that add_disk_task has started (if it
    // hasn't already); (if add_disk_task never ran, add_disk() would never be
    // called and del_gendisk() in device destruction would mess up, as
    // kthread_stop() will make the thread not run at all if it still hadn't
    // started running)

    wait_for_completion(&add_disk_task_started);

    // success

    return device;

    // failure

error_stop_add_disk_task:
    kthread_stop(device->add_disk_task);
error_cleanup_queue:
    blk_cleanup_queue(device->queue);
    blk_mq_free_tag_set(&device->tag_set);
error_destroy_inverter:
    kbdus_inverter_terminate(device->inverter);
    kbdus_inverter_destroy(device->inverter);
error_free_device:
    kfree(device);
error:
    return ERR_PTR(ret_error);
}

void kbdus_device_destroy(struct kbdus_device *device)
{
    int i;
    struct gendisk *disk;

    // terminate protocol instance (which fails all pending and future requests)

    kbdus_inverter_terminate(device->inverter);

    // wait for add_disk_task to end

    kthread_stop(device->add_disk_task);
    put_task_struct(device->add_disk_task);

    // delete gendisk and clean up queue (must be in this order, it appears)

    del_gendisk(device->disk);
    blk_cleanup_queue(device->queue);

    // ensure that no ioctl calls are running on the inverter

    for (i = 0; i < device->max_active_ioctl_reqs; ++i)
        down(&device->ioctl_semaphore);

    // destroy inverter

    kbdus_inverter_destroy(device->inverter);
    device->inverter = NULL; // ioctl submission checks if inverter is NULL

    // release ioctl semaphore

    for (i = 0; i < device->max_active_ioctl_reqs; ++i)
        up(&device->ioctl_semaphore);

    // free tag set

    blk_mq_free_tag_set(&device->tag_set);

    // must store `device->disk` as `kbdus_device_put_ref_()` may free `device`

    disk = device->disk;

    // free device structure if holding last reference (must do this before
    // put_disk() as the latter may invalidate `device->disk`)

    kbdus_device_put_ref_(disk);

    // put gendisk

    put_disk(disk);
}

enum kbdus_device_state kbdus_device_get_state(
    const struct kbdus_device *device
    )
{
    return atomic_read(&device->state);
}

const struct kbdus_device_config *kbdus_device_get_config(
    const struct kbdus_device *device
    )
{
    return &device->config;
}

bool kbdus_device_is_read_only(const struct kbdus_device *device)
{
    return kbdus_device_is_read_only_(&device->config);
}

struct block_device *kbdus_device_get_block_device(struct kbdus_device *device)
{
    return bdget_disk(device->disk, 0);
}

void kbdus_device_terminate(struct kbdus_device *device)
{
    atomic_set(&device->state, KBDUS_DEVICE_STATE_TERMINATED);
    kbdus_inverter_terminate(device->inverter);
}

void kbdus_device_deactivate(struct kbdus_device *device, bool flush)
{
    enum kbdus_device_state old_state;

    old_state = atomic_xchg(&device->state, KBDUS_DEVICE_STATE_INACTIVE);

    kbdus_assert(old_state == KBDUS_DEVICE_STATE_ACTIVE);

    kbdus_inverter_deactivate(device->inverter, flush);
}

void kbdus_device_activate(struct kbdus_device *device)
{
    enum kbdus_device_state old_state;

    old_state = atomic_xchg(&device->state, KBDUS_DEVICE_STATE_ACTIVE);

    kbdus_assert(old_state == KBDUS_DEVICE_STATE_INACTIVE);

    kbdus_inverter_activate(device->inverter);
    kbdus_inverter_submit_device_available_request(device->inverter);
}

ssize_t kbdus_device_handle_control_read_iter(
    struct kbdus_device *device,
    const struct kbdus_protocol *protocol,
    struct kbdus_protocol_instance *protocol_instance,
    struct iov_iter *to
    )
{
    return kbdus_protocol_handle_control_read_iter(
        protocol, protocol_instance, device->inverter, to
        );
}

ssize_t kbdus_device_handle_control_write_iter(
    struct kbdus_device *device,
    const struct kbdus_protocol *protocol,
    struct kbdus_protocol_instance *protocol_instance,
    struct iov_iter *from
    )
{
    return kbdus_protocol_handle_control_write_iter(
        protocol, protocol_instance, device->inverter, from
        );
}

int kbdus_device_handle_control_mmap(
    struct kbdus_device *device,
    const struct kbdus_protocol *protocol,
    struct kbdus_protocol_instance *protocol_instance,
    struct vm_area_struct *vma
    )
{
    return kbdus_protocol_handle_control_mmap(
        protocol, protocol_instance, device->inverter, vma
        );
}

int kbdus_device_handle_control_ioctl(
    struct kbdus_device *device,
    const struct kbdus_protocol *protocol,
    struct kbdus_protocol_instance *protocol_instance,
    unsigned int command,
    void __user *argument_usrptr
    )
{
    return kbdus_protocol_handle_control_ioctl(
        protocol, protocol_instance, device->inverter, command, argument_usrptr
        );
}

/* -------------------------------------------------------------------------- */
