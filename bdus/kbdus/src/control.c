/* -------------------------------------------------------------------------- */
/* includes */

#include <kbdus.h>
#include <kbdus/config.h>
#include <kbdus/control.h>
#include <kbdus/device.h>
#include <kbdus/utilities.h>

#include <linux/aio.h>
#include <linux/bitops.h>
#include <linux/capability.h>
#include <linux/cdev.h>
#include <linux/dcache.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/stddef.h>
#include <linux/stringify.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/uio.h>
#include <linux/wait.h>

/* -------------------------------------------------------------------------- */
/* module parameters */

static unsigned int kbdus_control_max_devices_ =
    (unsigned int)KBDUS_HARD_MAX_DEVICES;

module_param_named(max_devices, kbdus_control_max_devices_, uint, 0444);

MODULE_PARM_DESC(
    max_devices,
    "The maximum number of BDUS block devices that can exist simultaneously."
    " Must be positive and less than or equal to "
    __stringify(KBDUS_HARD_MAX_DEVICES) ". Defaults to "
    __stringify(KBDUS_HARD_MAX_DEVICES) "."
    );

static int validate_module_parameters_(void)
{
    // validate module parameter 'max_devices'

    if (kbdus_control_max_devices_ < 1u ||
        kbdus_control_max_devices_ > (unsigned int)KBDUS_HARD_MAX_DEVICES)
    {
        printk(
            KERN_ERR "kbdus: Invalid value %u for module parameter"
            " 'max_devices', must be positive and less than or equal to"
            " " __stringify(KBDUS_HARD_MAX_DEVICES) ".\n",
            kbdus_control_max_devices_
            );

        return -EINVAL;
    }

    // parameters are valid

    return 0;
}

/* -------------------------------------------------------------------------- */
/* private -- constants */

// kbdus' version.
static const struct kbdus_version kbdus_control_version_ = {
    .major = (u32)BDUS_VERSION_MAJOR,
    .minor = (u32)BDUS_VERSION_MINOR,
    .patch = (u32)BDUS_VERSION_PATCH,
};

// Name of the control device (which, in particular, shows up in /dev when using
// udev).
#define KBDUS_CONTROL_DEVICE_NAME_ "bdus-control"

// Name of the sysfs class of the control device.
#define KBDUS_CONTROL_CLASS_NAME_ "bdus-control"

enum
{
    // Index for control session flag which is set if the session has a device
    // associated with it.
    KBDUS_CONTROL_SESSION_FLAG_ATTACHED_,

    // Whether the session was "marked as successful" through the
    // KBDUS_IOCTL_MARK_AS_SUCCESSFUL ioctl.
    KBDUS_CONTROL_SESSION_FLAG_SUCCESSFUL_,
};

/* -------------------------------------------------------------------------- */
/* private -- types */

/**
 * struct kbdus_control_device_wrapper_ - TODO.
 */
struct kbdus_control_device_wrapper_
{
    /** @list: Used to store the device in `kbdus_control_devices_`. */
    struct list_head list;

    /** @device: The device. */
    struct kbdus_device *device;

    /**
     * @session: The session that is attached to this device, or NULL if no
     *     session is attached to this device.
     */
    struct kbdus_control_session_ *session;

    /** @on_detach: TODO. */
    struct completion *on_detach;
};

/**
 * struct kbdus_control_session_ - State of a "control session", created when
 *     the control device is opened and destroyed when all corresponding file
 *     descriptors are closed.
 */
struct kbdus_control_session_
{
    /** @flags: Atomic bitmask. */
    unsigned long flags;

    /**
     * @device_wrapper: The device to which this session is attached. Valid only
     *     if flags has bit KBDUS_CONTROL_SESSION_FLAG_ATTACHED_ set.
     */
    struct kbdus_control_device_wrapper_ *device_wrapper;

    /**
     * @protocol: TODO. Valid only if flags has bit
     *     KBDUS_CONTROL_SESSION_FLAG_ATTACHED_ set.
     */
    const struct kbdus_protocol *protocol;
    /**
     * @protocol_instance: Valid only if flags has bit
     *     KBDUS_CONTROL_SESSION_FLAG_ATTACHED_ set.
     */
    struct kbdus_protocol_instance *protocol_instance;
};

/* -------------------------------------------------------------------------- */
/* private -- variables */

// Some necessary control device state.
static dev_t kbdus_control_majorminor_;
static struct cdev kbdus_control_cdev_;
static struct class *kbdus_control_class_;
static struct device *kbdus_control_device_;

// TODO: document
static DEFINE_MUTEX(kbdus_control_mutex_);

/*
 * The seqnum for the next device being created.
 *
 * Always modified with both `kbdus_control_mutex_` and
 * `kbdus_control_device_destroyed_.lock` held.
 */
static u64 kbdus_control_next_seqnum_ = 0;

/*
 * A list of device wrappers for all existing devices.
 *
 * Always sorted by device index.
 *
 * Always modified with both `kbdus_control_mutex_` and
 * `kbdus_control_device_destroyed_.lock` held.
 */
static LIST_HEAD(kbdus_control_devices_);

// Triggered whenever a device is destroyed, after its wrapper is removed from
// `kbdus_control_devices_`.
static DECLARE_WAIT_QUEUE_HEAD(kbdus_control_device_destroyed_);

/* -------------------------------------------------------------------------- */
/* private -- utility functions */

#ifdef CONFIG_BLK_DEBUG_FS

/**
 * kbdus_control_debugfs_entry_exists_() - TODO.
 * @block_debugfs_dir: TODO.
 * @device_index: TODO.
 *
 * TODO.
 *
 * Must be called with `kbdus_control_mutex_` held.
 *
 * Might sleep.
 */
static bool kbdus_control_debugfs_entry_exists_(
    struct dentry *block_debugfs_dir,
    u32 device_index
    )
{
    // Note: If debugfs exists, must avoid reusing BDUS device name while
    // debugfs entry for it exists. The debugfs entry is removed when request
    // queue is released. However, request queue might keep on existing after
    // destroying device because e.g. open file descriptions to the device. As
    // far as I can tell, there is no good way to check or be notified whether
    // the queue when the queue is released. Thus, we simply check whether the
    // debugfs entry still exists, and avoid reusing the device index while it
    // does. (TODO: there must be a better way)

    char device_name[DISK_NAME_LEN];
    int ret;
    struct dentry *dir_device;

    ret = snprintf(device_name, DISK_NAME_LEN, "bdus-%u", device_index);
    kbdus_assert(ret < DISK_NAME_LEN);

    dir_device = debugfs_lookup(device_name, block_debugfs_dir);

    if (dir_device)
    {
        dput(dir_device);
        return true;
    }
    else
    {
        return false;
    }
}

#endif

/**
 * kbdus_control_find_unused_index_() - Find an unused device index.
 * @out_index: TODO.
 * @out_list_prev: TODO.
 *
 * The unused device index is returned in `*out_index`, and the entry in the
 * `kbdus_control_devices_` list after which the new entry should be inserted
 * (to keep the list sorted by device index) is returned in `*out_list_prev`.
 *
 * Must be called with `kbdus_control_mutex_` held.
 *
 * Might sleep.
 */
static bool kbdus_control_find_unused_index_(
    u32 *out_index,
    struct list_head **out_list_prev
    )
{
#ifdef CONFIG_BLK_DEBUG_FS
    struct dentry *block_debugfs_dir;
#endif

    bool ret;
    struct kbdus_control_device_wrapper_ *wrapper;
    u32 i;

    // get "block" debugfs directory

#ifdef CONFIG_BLK_DEBUG_FS
    block_debugfs_dir = debugfs_lookup("block", NULL);
    kbdus_assert(block_debugfs_dir);
#endif

    // find unused index

    ret = false;

    wrapper = list_first_entry_or_null(
        &kbdus_control_devices_, struct kbdus_control_device_wrapper_, list
        );

    for (i = 0; i < (u32)kbdus_control_max_devices_; ++i)
    {
        if (wrapper && i == kbdus_device_get_config(wrapper->device)->index)
        {
            // candidate index is already used, increment candidate
            // index and move to next device wrapper in device list

            if (wrapper->list.next == &kbdus_control_devices_)
                wrapper = NULL; // no more device wrappers in dev list
            else
                wrapper = list_next_entry(wrapper, list);

            continue;
        }

#ifdef CONFIG_BLK_DEBUG_FS
        if (kbdus_control_debugfs_entry_exists_(block_debugfs_dir, i))
            continue; // debugfs entry for device still exists
#endif

        // found unused index

        *out_index = i;

        *out_list_prev =
            wrapper
                ? wrapper->list.prev
                : kbdus_control_devices_.prev;

        ret = true;
        break;
    }

    // put "block" debugfs directory

#ifdef CONFIG_BLK_DEBUG_FS
    dput(block_debugfs_dir);
#endif

    // return result

    return ret;
}

/**
 * kbdus_control_lookup_device_wrapper_by_index_() - Get the device wrapper for
 *     the device with the given index.
 * @index: TODO.
 *
 * Returns NULL if no such device exists.
 *
 * Must be called with `kbdus_control_mutex_` or
 * `kbdus_control_device_destroyed_.lock` (or both) held.
 *
 * Never sleeps.
 *
 * Return: TODO
 */
static struct kbdus_control_device_wrapper_ *
    kbdus_control_lookup_device_wrapper_by_index_(u32 index)
{
    struct kbdus_control_device_wrapper_ *device_wrapper;
    const struct kbdus_device_config *device_config;

    list_for_each_entry(device_wrapper, &kbdus_control_devices_, list)
    {
        device_config = kbdus_device_get_config(device_wrapper->device);

        if (device_config->index == index)
            return device_wrapper;
        else if (device_config->index > index)
            break;
    }

    return NULL;
}

/**
 * kbdus_control_lookup_device_wrapper_by_seqnum_() - Get the device wrapper for
 *     the device with the given seqnum.
 * @seqnum: TODO.
 *
 * Returns NULL if no such device exists.
 *
 * Must be called with `kbdus_control_mutex_` or
 * `kbdus_control_device_destroyed_.lock` (or both) held.
 *
 * Never sleeps.
 *
 * Return: TODO
 */
static struct kbdus_control_device_wrapper_ *
    kbdus_control_lookup_device_wrapper_by_seqnum_(u64 seqnum)
{
    struct kbdus_control_device_wrapper_ *device_wrapper;
    const struct kbdus_device_config *device_config;

    list_for_each_entry(device_wrapper, &kbdus_control_devices_, list)
    {
        device_config = kbdus_device_get_config(device_wrapper->device);

        if (device_config->seqnum == seqnum)
            return device_wrapper;
    }

    return NULL;
}

/**
 * kbdus_control_destroy_device_() - Destroy a device.
 * @device_wrapper: TODO.
 *
 * The device wrapper is first removed from the `kbdus_control_devices_` list
 * and then the device is destroyed and the device wrapper is freed.
 *
 * Must be called with `kbdus_control_mutex_` held. Acquires and releases
 * `kbdus_control_device_destroyed_.lock`.
 *
 * Might sleep.
 */
static void kbdus_control_destroy_device_(
    struct kbdus_control_device_wrapper_ *device_wrapper
    )
{
    // remove device wrapper from list of devices and notify waiters

    spin_lock(&kbdus_control_device_destroyed_.lock);

    list_del(&device_wrapper->list);
    wake_up_all_locked(&kbdus_control_device_destroyed_);

    spin_unlock(&kbdus_control_device_destroyed_.lock);

    // destroy device

    kbdus_device_destroy(device_wrapper->device);

    // free device wrapper

    kfree(device_wrapper);
}

/**
 * kbdus_control_flush_bdev_() - Flush the given `block_device`.
 * @bdev: TODO.
 *
 * This function is similar in function to `blkdev_fsync()`.
 *
 * Flushes the page cache of the given `block_device` (converting dirty pages
 * into "write" requests) and then sends it a "flush" request.
 *
 * Return: TODO.
 */
static int kbdus_control_flush_bdev_(struct block_device *bdev)
{
    int ret;

    // open block_device

    bdgrab(bdev);

    if ((ret = blkdev_get(bdev, FMODE_READ, NULL)) != 0)
        return ret;

    // flush block_device (similar to what is done in blkdev_fsync())

    kbdus_assert(bdev->bd_inode);
    kbdus_assert(bdev->bd_inode->i_mapping);

    ret = filemap_write_and_wait(bdev->bd_inode->i_mapping);

    if (ret == 0)
    {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,8,0)
        ret = blkdev_issue_flush(bdev, GFP_KERNEL);
#else
        ret = blkdev_issue_flush(bdev, GFP_KERNEL, NULL);
#endif

        if (ret == -EOPNOTSUPP)
            ret = 0;
    }

    // close block_device

    blkdev_put(bdev, FMODE_READ);

    // return result

    return ret;
}

/* -------------------------------------------------------------------------- */
/* private -- control device operations */

/**
 * kbdus_control_open_() - Implements the `open` file operation of the control
 *     device.
 * @inode: TODO.
 * @filp: TODO.
 *
 * Return: TODO.
 */
static int kbdus_control_open_(struct inode *inode, struct file *filp)
{
    struct kbdus_control_session_ *session;

    // set mode (same as stream_open(), which only exists since Linux 5.0.14)

    filp->f_mode &=
        ~(FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE | FMODE_ATOMIC_POS);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,14)
    filp->f_mode |= FMODE_STREAM;
#endif

    // check capabilities

    if (!capable(CAP_SYS_ADMIN))
        return -EACCES;

    // allocate session

    session = kmalloc(sizeof(*session), GFP_KERNEL);

    if (!session)
        return -ENOMEM;

    // initialize session

    clear_bit(KBDUS_CONTROL_SESSION_FLAG_ATTACHED_, &session->flags);
    clear_bit(KBDUS_CONTROL_SESSION_FLAG_SUCCESSFUL_, &session->flags);

    // store session in filp's private data

    filp->private_data = session;

    // success

    return 0;
}

/**
 * kbdus_control_release_() - The control device's `release` file operation.
 * @inode: TODO.
 * @filp: TODO.
 *
 * Return: TODO.
 */
static int kbdus_control_release_(struct inode *inode, struct file *filp)
{
    struct kbdus_control_session_ *session;
    const struct kbdus_device_config *device_config;

    session = filp->private_data;

    if (test_bit(KBDUS_CONTROL_SESSION_FLAG_ATTACHED_, &session->flags))
    {
        mutex_lock(&kbdus_control_mutex_);

        device_config = kbdus_device_get_config(
            session->device_wrapper->device
            );

        session->device_wrapper->session = NULL;

        switch (kbdus_device_get_state(session->device_wrapper->device))
        {
        case KBDUS_DEVICE_STATE_UNAVAILABLE:

            kbdus_assert(!session->device_wrapper->on_detach);

            kbdus_control_destroy_device_(session->device_wrapper);

            break;

        case KBDUS_DEVICE_STATE_ACTIVE:

            if (device_config->destroy_when_detached && !test_bit(
                KBDUS_CONTROL_SESSION_FLAG_SUCCESSFUL_, &session->flags
                ))
            {
                kbdus_device_terminate(session->device_wrapper->device);
            }
            else
            {
                kbdus_device_deactivate(session->device_wrapper->device, false);
            }

            if (session->device_wrapper->on_detach)
                complete(session->device_wrapper->on_detach);
            else if (device_config->destroy_when_detached)
                kbdus_control_destroy_device_(session->device_wrapper);

            break;

        case KBDUS_DEVICE_STATE_INACTIVE:

            if (device_config->destroy_when_detached && !test_bit(
                KBDUS_CONTROL_SESSION_FLAG_SUCCESSFUL_, &session->flags
                ))
            {
                kbdus_device_terminate(session->device_wrapper->device);
            }

            if (session->device_wrapper->on_detach)
                complete(session->device_wrapper->on_detach);
            else if (device_config->destroy_when_detached)
                kbdus_control_destroy_device_(session->device_wrapper);

            break;

        case KBDUS_DEVICE_STATE_TERMINATED:

            if (session->device_wrapper->on_detach)
                complete(session->device_wrapper->on_detach);
            else
                kbdus_control_destroy_device_(session->device_wrapper);

            break;

        default:
            kbdus_assert(false);
            break;
        }

        mutex_unlock(&kbdus_control_mutex_);
    }

    kfree(session);

    return 0;
}

/**
 * kbdus_control_read_iter_() - Implements the `read_iter` file operation for
 *     the control device.
 * @iocb: TODO.
 * @to: TODO.
 *
 * Return: TODO.
 */
static ssize_t kbdus_control_read_iter_(
    struct kiocb *iocb, struct iov_iter *to
    )
{
    struct kbdus_control_session_ *session;

    session = iocb->ki_filp->private_data;

    if (!test_bit(KBDUS_CONTROL_SESSION_FLAG_ATTACHED_, &session->flags))
        return -EINVAL;

    return kbdus_device_handle_control_read_iter(
        session->device_wrapper->device,
        session->protocol,
        session->protocol_instance,
        to
        );
}

/**
 * kbdus_control_write_iter_() - Implements the `write_iter` file operation for
 *     the control device.
 * @iocb: TODO.
 * @from: TODO.
 *
 * Return: TODO.
 */
static ssize_t kbdus_control_write_iter_(
    struct kiocb *iocb, struct iov_iter *from
    )
{
    struct kbdus_control_session_ *session;

    session = iocb->ki_filp->private_data;

    // ensure that session is associated with a device

    if (!test_bit(KBDUS_CONTROL_SESSION_FLAG_ATTACHED_, &session->flags))
        return -EINVAL;

    // delegate write to device

    return kbdus_device_handle_control_write_iter(
        session->device_wrapper->device,
        session->protocol,
        session->protocol_instance,
        from
        );
}

/**
 * kbdus_control_mmap_() - Implements the `mmap` file operation of the control
 *     device.
 * @filp: TODO.
 * @vma: TODO.
 *
 * Return: TODO.
 */
static int kbdus_control_mmap_(struct file *filp, struct vm_area_struct *vma)
{
    struct kbdus_control_session_ *session;

    session = filp->private_data;

    // ensure that session is associated with a device

    if (!test_bit(KBDUS_CONTROL_SESSION_FLAG_ATTACHED_, &session->flags))
        return -EINVAL;

    // delegate mmap to device

    return kbdus_device_handle_control_mmap(
        session->device_wrapper->device,
        session->protocol,
        session->protocol_instance,
        vma
        );
}

/**
 * kbdus_control_ioctl_get_version_() - Implements the `KBDUS_IOCTL_GET_VERSION`
 *     ioctl command for the `unlocked_ioctl` and `compat_ioctl` file operations
 *     of the control device.
 * @version_usrptr: TODO.
 *
 * Return: TODO.
 */
static int kbdus_control_ioctl_get_version_(
    struct kbdus_version __user *version_usrptr
    )
{
    // copy version to user space

    if (copy_to_user(
        version_usrptr, &kbdus_control_version_, sizeof(kbdus_control_version_)
        ) != 0)
    {
        return -EFAULT;
    }

    // success

    return 0;
}

/**
 * __kbdus_control_ioctl_create_device_() - TODO.
 * @session: TODO.
 * @protocol: TODO.
 * @config: TODO.
 * @config_usrptr: TODO.
 *
 * Must be called with `kbdus_control_mutex_` held.
 *
 * Return: TODO.
 */
static int __kbdus_control_ioctl_create_device_(
    struct kbdus_control_session_ *session,
    const struct kbdus_protocol *protocol,
    struct kbdus_config *config,
    struct kbdus_config __user *config_usrptr
    )
{
    struct list_head *device_wrapper_list_prev;
    struct kbdus_control_device_wrapper_ *device_wrapper;
    struct kbdus_protocol_instance *protocol_instance;
    struct kbdus_device *device;

    // ensure that session is attached to a device

    if (test_bit(KBDUS_CONTROL_SESSION_FLAG_ATTACHED_, &session->flags))
        return -EINVAL;

    // get index for device

    if (!kbdus_control_find_unused_index_(
        &config->device.index, &device_wrapper_list_prev
        ))
    {
        return -ENOSPC; // too many devices
    }

    config->device.seqnum      = kbdus_control_next_seqnum_;
    config->device.first_minor = config->device.index * (u32)DISK_MAX_PARTS;

    // copy modified device configuration back to user space

    if (__copy_to_user(
        &config_usrptr->device, &config->device, sizeof(config->device)
        ) != 0)
    {
        return -EFAULT;
    }

    // create device wrapper

    device_wrapper = kmalloc(sizeof(*device_wrapper), GFP_KERNEL);

    if (!device_wrapper)
        return -ENOMEM;

    // create protocol instance

    protocol_instance = kbdus_protocol_create_instance(protocol, config);

    if (IS_ERR(protocol_instance))
    {
        kfree(device_wrapper);
        return PTR_ERR(protocol_instance);
    }

    // create device

    device = kbdus_device_create(&config->device);

    if (IS_ERR(device))
    {
        kbdus_protocol_destroy_instance(protocol, protocol_instance);
        kfree(device_wrapper);
        return PTR_ERR(device);
    }

    // associate device with session

    spin_lock(&kbdus_control_device_destroyed_.lock);

    ++kbdus_control_next_seqnum_;

    list_add(&device_wrapper->list, device_wrapper_list_prev);
    device_wrapper->device    = device;
    device_wrapper->session   = session;
    device_wrapper->on_detach = NULL;

    spin_unlock(&kbdus_control_device_destroyed_.lock);

    session->device_wrapper    = device_wrapper;
    session->protocol          = protocol;
    session->protocol_instance = protocol_instance;
    set_bit(KBDUS_CONTROL_SESSION_FLAG_ATTACHED_, &session->flags);

    // success

    return 0;
}

/**
 * kbdus_control_ioctl_create_device_() - Implements the
 *     `KBDUS_IOCTL_CREATE_DEVICE` ioctl command for the `unlocked_ioctl` and
 *     `compat_ioctl` file operations of the control device.
 * @session: TODO.
 * @config_usrptr: TODO.
 *
 * Return: TODO.
 */
static int kbdus_control_ioctl_create_device_(
    struct kbdus_control_session_ *session,
    struct kbdus_config __user *config_usrptr
    )
{
    struct kbdus_config config;
    const struct kbdus_protocol *protocol;
    int ret;

    // validate user-space pointer

    if (!kbdus_access_ok(KBDUS_VERIFY_WRITE, config_usrptr, sizeof(config)))
        return -EFAULT;

    // copy device configuration from user space

    if (__copy_from_user(&config, config_usrptr, sizeof(config)) != 0)
        return -EFAULT;

    // validate and adjust device configuration

    if ((ret = kbdus_device_validate_and_adjust_config(&config.device)) != 0)
        return ret;

    // lookup protocol

    protocol = kbdus_protocol_lookup(config.protocol.name);

    if (!protocol)
        return -EINVAL;

    // validate protocol configuration

    if (!kbdus_protocol_validate_config(protocol, &config))
        return -EINVAL;

    // lock global mutex

    if (mutex_lock_interruptible(&kbdus_control_mutex_) != 0)
        return -ERESTARTSYS;

    // delegate remaining work

    ret = __kbdus_control_ioctl_create_device_(
        session, protocol, &config, config_usrptr
        );

    // unlock global mutex

    mutex_unlock(&kbdus_control_mutex_);

    // return result

    return ret;
}

/**
 * __kbdus_control_ioctl_attach_to_device_() - TODO.
 * @config_usrptr: TODO.
 * @session: TODO.
 * @protocol: TODO.
 * @protocol_config: TODO.
 * @seqnum: TODO.
 *
 * Must be called with `kbdus_control_mutex_` held.
 *
 * Return: TODO.
 */
static int __kbdus_control_ioctl_attach_to_device_(
    struct kbdus_config __user *config_usrptr,
    struct kbdus_control_session_ *session,
    const struct kbdus_protocol *protocol,
    const struct kbdus_protocol_config *protocol_config,
    u64 seqnum
    )
{
    struct kbdus_control_device_wrapper_ *device_wrapper;
    struct completion on_detach;
    struct kbdus_config config;
    struct kbdus_protocol_instance *protocol_instance;

    // ensure that session is not associated with a device

    if (test_bit(KBDUS_CONTROL_SESSION_FLAG_ATTACHED_, &session->flags))
        return -EINVAL;

    // check if seqnum was ever used

    if (seqnum >= kbdus_control_next_seqnum_)
        return -EINVAL;

    // get device wrapper (if device with given seqnum exists)

    device_wrapper = kbdus_control_lookup_device_wrapper_by_seqnum_(seqnum);

    if (!device_wrapper)
        return -ENODEV;

    // get configuration

    config.device   = *kbdus_device_get_config(device_wrapper->device);
    config.protocol = *protocol_config;

    // validate protocol configuration

    if (!kbdus_protocol_validate_config(protocol, &config))
        return -EINVAL;

    // ensure that device is available

    if (kbdus_device_get_state(device_wrapper->device) ==
        KBDUS_DEVICE_STATE_UNAVAILABLE)
    {
        return -EBUSY;
    }

    // ensure that no one else is trying to attach to the device

    if (device_wrapper->on_detach)
        return -EINPROGRESS;

    // copy device configuration to user space

    if (__copy_to_user(
        &config_usrptr->device, &config.device, sizeof(config_usrptr->device)
        ) != 0)
    {
        return -EFAULT;
    }

    // check if device has session attached to it

    if (device_wrapper->session)
    {
        // deactivate device (unless already terminated, in which case we use
        // the completion simply to wait until the device is destroyed)

        if (kbdus_device_get_state(device_wrapper->device)
            != KBDUS_DEVICE_STATE_TERMINATED)
        {
            kbdus_device_deactivate(device_wrapper->device, true);
        }

        // wait until attached session detaches from device

        init_completion(&on_detach);
        device_wrapper->on_detach = &on_detach;

        mutex_unlock(&kbdus_control_mutex_);

        if (wait_for_completion_interruptible(&on_detach) == 0)
        {
            // waited until session detached

            mutex_lock(&kbdus_control_mutex_);

            device_wrapper->on_detach = NULL;

            kbdus_assert(!device_wrapper->session);
        }
        else
        {
            // wait interrupted, session may or may not have detached from
            // device

            mutex_lock(&kbdus_control_mutex_);

            device_wrapper->on_detach = NULL;

            if (device_wrapper->session)
                return -ERESTARTSYS;
        }

        // destroy device if it was already terminated

        if (kbdus_device_get_state(device_wrapper->device) ==
            KBDUS_DEVICE_STATE_TERMINATED)
        {
            kbdus_control_destroy_device_(device_wrapper);
            return -ENODEV;
        }
    }

    kbdus_assert(
        kbdus_device_get_state(device_wrapper->device)
            == KBDUS_DEVICE_STATE_INACTIVE
        );

    // create protocol instance

    protocol_instance = kbdus_protocol_create_instance(protocol, &config);

    if (IS_ERR(protocol_instance))
    {
        if (config.device.destroy_when_detached)
            kbdus_control_destroy_device_(device_wrapper);

        return PTR_ERR(protocol_instance);
    }

    // activate device

    kbdus_device_activate(device_wrapper->device);

    // attach session to device

    device_wrapper->session = session;

    session->device_wrapper    = device_wrapper;
    session->protocol          = protocol;
    session->protocol_instance = protocol_instance;

    set_bit(KBDUS_CONTROL_SESSION_FLAG_ATTACHED_, &session->flags);

    // success

    return 0;
}

/**
 * kbdus_control_ioctl_attach_to_device_() - Implements the
 *     `KBDUS_IOCTL_ATTACH_TO_DEVICE` ioctl command for the `unlocked_ioctl` and
 *     `compat_ioctl` file operations of the control device.
 * @session: TODO.
 * @config_usrptr: TODO.
 *
 * Return: TODO.
 */
static int kbdus_control_ioctl_attach_to_device_(
    struct kbdus_control_session_ *session,
    struct kbdus_config __user *config_usrptr
    )
{
    u64 seqnum;
    struct kbdus_protocol_config protocol_config;
    const struct kbdus_protocol *protocol;
    int ret;

    // validate user-space pointer

    if (!kbdus_access_ok(
        KBDUS_VERIFY_WRITE, config_usrptr, sizeof(*config_usrptr)
        ))
    {
        return -EFAULT;
    }

    // copy device seqnum from user space

    if (__get_user(seqnum, &config_usrptr->device.seqnum) != 0)
        return -EFAULT;

    // copy protocol configuration from user space

    if (__copy_from_user(
        &protocol_config, &config_usrptr->protocol, sizeof(protocol_config)
        ) != 0)
    {
        return -EFAULT;
    }

    // lookup protocol

    protocol = kbdus_protocol_lookup(protocol_config.name);

    if (!protocol)
        return -EINVAL;

    // lock global mutex

    if (mutex_lock_interruptible(&kbdus_control_mutex_) != 0)
        return -ERESTARTSYS;

    // delegate remaining work

    ret = __kbdus_control_ioctl_attach_to_device_(
        config_usrptr, session, protocol, &protocol_config, seqnum
        );

    // unlock global mutex

    mutex_unlock(&kbdus_control_mutex_);

    // return result

    return ret;
}

/**
 * kbdus_control_ioctl_mark_as_successful_() - Implements the
 *     `KBDUS_IOCTL_MARK_AS_SUCCESSFUL` ioctl command for the `unlocked_ioctl`
 *     and `compat_ioctl` file operations of the control device.
 * @session: TODO.
 *
 * Return: TODO.
 */
static int kbdus_control_ioctl_mark_as_successful_(
    struct kbdus_control_session_ *session
    )
{
    set_bit(KBDUS_CONTROL_SESSION_FLAG_SUCCESSFUL_, &session->flags);
    return 0;
}

/**
 * kbdus_control_ioctl_device_index_to_seqnum_() - Implements the
 *     `KBDUS_IOCTL_DEVICE_INDEX_TO_SEQNUM` ioctl command for the
 *     `unlocked_ioctl` and `compat_ioctl` file operations of the control
 *     device.
 * @index64_or_seqnum_usrptr: TODO.
 *
 * Return: TODO.
 */
static int kbdus_control_ioctl_device_index_to_seqnum_(
    u64 __user *index64_or_seqnum_usrptr
    )
{
    u64 index64;
    struct kbdus_control_device_wrapper_ *device_wrapper;
    u64 seqnum;

    // validate user-space pointer

    if (!kbdus_access_ok(
        KBDUS_VERIFY_WRITE, index64_or_seqnum_usrptr, sizeof(u64)
        ))
    {
        return -EFAULT;
    }

    // copy device index from user space

    if (__get_user(index64, index64_or_seqnum_usrptr) != 0)
        return -EFAULT;

    // validate device index

    if (index64 > (u64)U32_MAX)
        return -EINVAL;

    // lock global mutex

    if (mutex_lock_interruptible(&kbdus_control_mutex_) != 0)
        return -ERESTARTSYS;

    // get session associated with the device

    device_wrapper = kbdus_control_lookup_device_wrapper_by_index_(
        (u32)index64
        );

    if (!device_wrapper)
    {
        mutex_unlock(&kbdus_control_mutex_);
        return -ENODEV;
    }

    // store seqnum (as device may be destroyed after unlocking the global
    // mutex)

    seqnum = kbdus_device_get_config(device_wrapper->device)->seqnum;

    // unlock global mutex

    mutex_unlock(&kbdus_control_mutex_);

    // copy device seqnum to user space

    if (__put_user(seqnum, index64_or_seqnum_usrptr) != 0)
        return -EFAULT;

    // success

    return 0;
}

/**
 * kbdus_control_ioctl_get_device_config_() - Implements the
 *     `KBDUS_IOCTL_GET_DEVICE_CONFIG` ioctl command for the `unlocked_ioctl`
 *     and `compat_ioctl` file operations of the control device.
 * @device_config_usrptr: TODO.
 *
 * Return: TODO.
 */
static int kbdus_control_ioctl_get_device_config_(
    struct kbdus_device_config __user *device_config_usrptr
    )
{
    u64 seqnum;
    int ret;
    struct kbdus_control_device_wrapper_ *device_wrapper;

    // validate user-space pointer

    if (!kbdus_access_ok(
        KBDUS_VERIFY_WRITE, device_config_usrptr, sizeof(*device_config_usrptr)
        ))
    {
        return -EFAULT;
    }

    // copy device seqnum from user space

    if (__get_user(seqnum, &device_config_usrptr->seqnum) != 0)
        return -EFAULT;

    // lock global mutex

    if (mutex_lock_interruptible(&kbdus_control_mutex_) != 0)
        return -ERESTARTSYS;

    // check if seqnum was ever used

    if (seqnum >= kbdus_control_next_seqnum_)
    {
        ret = -EINVAL;
        goto out_unlock;
    }

    // get device wrapper

    device_wrapper = kbdus_control_lookup_device_wrapper_by_seqnum_(seqnum);

    if (!device_wrapper)
    {
        ret = -ENODEV;
        goto out_unlock;
    }

    // copy device configuration to user-space

    if (__copy_to_user(
        device_config_usrptr,
        kbdus_device_get_config(device_wrapper->device),
        sizeof(*device_config_usrptr)
        ) != 0)
    {
        ret = -EFAULT;
        goto out_unlock;
    }

    // success

    ret = 0;

out_unlock:
    mutex_unlock(&kbdus_control_mutex_);
    return ret;
}

/**
 * kbdus_control_ioctl_flush_device_() - Implements the
 *     `KBDUS_IOCTL_FLUSH_DEVICE` ioctl command for the `unlocked_ioctl` and
 *     `compat_ioctl` file operations of the control device.
 * @seqnum_usrptr: TODO.
 *
 * Return: TODO.
 */
static int kbdus_control_ioctl_flush_device_(const u64 __user *seqnum_usrptr)
{
    u64 seqnum;
    struct kbdus_control_device_wrapper_ *device_wrapper;
    struct kbdus_device_config *device_config;
    struct block_device *bdev;
    int ret;

    // copy device seqnum from user space

    if (get_user(seqnum, seqnum_usrptr) != 0)
        return -EFAULT;

    // lock global mutex (so that device isn't destroyed while we are getting
    // the block_device)

    if (mutex_lock_interruptible(&kbdus_control_mutex_) != 0)
        return -ERESTARTSYS;

    // check if seqnum was ever used

    if (seqnum >= kbdus_control_next_seqnum_)
    {
        ret = -EINVAL;
        goto out_unlock;
    }

    // get device wrapper associated with the device

    device_wrapper = kbdus_control_lookup_device_wrapper_by_seqnum_(seqnum);

    if (!device_wrapper)
    {
        ret = -ENODEV;
        goto out_unlock;
    }

    // skip flush if device is read-only

    if (kbdus_device_is_read_only(device_wrapper->device))
    {
        ret = 0;
        goto out_unlock;
    }

    // get reference to the block_device corresponding to the whole device

    bdev = kbdus_device_get_block_device(device_wrapper->device);

    if (!bdev)
    {
        ret = -EIO;
        goto out_unlock;
    }

    // unlock global mutex (to avoid blocking others while we flush)

    mutex_unlock(&kbdus_control_mutex_);

    // flush block_device

    ret = kbdus_control_flush_bdev_(bdev);

    // put reference to block_device

    bdput(bdev);

    // return result

    goto out;

out_unlock:
    mutex_unlock(&kbdus_control_mutex_);
out:
    return ret;
}

/**
 * kbdus_control_ioctl_request_session_termination_() - Implements the
 *     `KBDUS_IOCTL_REQUEST_SESSION_TERMINATION` ioctl command for the
 *     `unlocked_ioctl` and `compat_ioctl` file operations of the control
 *     device.
 * @seqnum_usrptr: TODO.
 *
 * Return: TODO.
 */
static int kbdus_control_ioctl_request_session_termination_(
    const u64 __user *seqnum_usrptr
    )
{
    u64 seqnum;
    struct kbdus_control_device_wrapper_ *device_wrapper;

    if (get_user(seqnum, seqnum_usrptr) != 0)
        return -EFAULT;

    if (mutex_lock_interruptible(&kbdus_control_mutex_) != 0)
        return -ERESTARTSYS;

    if (seqnum >= kbdus_control_next_seqnum_)
    {
        /* seqnum was never used */

        mutex_unlock(&kbdus_control_mutex_);
        return -EINVAL;
    }

    device_wrapper = kbdus_control_lookup_device_wrapper_by_seqnum_(seqnum);

    if (device_wrapper)
    {
        switch (kbdus_device_get_state(device_wrapper->device))
        {
        case KBDUS_DEVICE_STATE_UNAVAILABLE:

            kbdus_device_terminate(device_wrapper->device);

            break;

        case KBDUS_DEVICE_STATE_ACTIVE:

            if (kbdus_device_get_config(device_wrapper->device)->destroy_when_detached)
                kbdus_device_terminate(device_wrapper->device);
            else
                kbdus_device_deactivate(device_wrapper->device, false);

            break;

        case KBDUS_DEVICE_STATE_INACTIVE:

            if (kbdus_device_get_config(device_wrapper->device)->destroy_when_detached)
                kbdus_device_terminate(device_wrapper->device);

            break;

        case KBDUS_DEVICE_STATE_TERMINATED:
            break;

        default:
            kbdus_assert(false);
            break;
        }
    }

    mutex_unlock(&kbdus_control_mutex_);

    return 0;
}

/**
 * kbdus_control_ioctl_request_device_destruction_() - Implements the
 *     `KBDUS_IOCTL_REQUEST_DEVICE_DESTRUCTION` ioctl command for the
 *     `unlocked_ioctl` and `compat_ioctl` file operations of the control
 *     device.
 * @seqnum_usrptr: TODO.
 *
 * Return: TODO.
 */
static int kbdus_control_ioctl_request_device_destruction_(
    const u64 __user *seqnum_usrptr
    )
{
    u64 seqnum;
    struct kbdus_control_device_wrapper_ *device_wrapper;

    // copy device seqnum from user space

    if (get_user(seqnum, seqnum_usrptr) != 0)
        return -EFAULT;

    // lock global mutex (so that device isn't destroyed while we are requesting
    // it to terminate)

    if (mutex_lock_interruptible(&kbdus_control_mutex_) != 0)
        return -ERESTARTSYS;

    // check if seqnum was ever used

    if (seqnum >= kbdus_control_next_seqnum_)
    {
        mutex_unlock(&kbdus_control_mutex_);
        return -EINVAL;
    }

    // get device (if it still exists)

    device_wrapper = kbdus_control_lookup_device_wrapper_by_seqnum_(seqnum);

    // terminate device (if it still exists) and destroy it if applicable

    if (device_wrapper)
    {
        if (device_wrapper->session)
            kbdus_device_terminate(device_wrapper->device);
        else
            kbdus_control_destroy_device_(device_wrapper);
    }

    // unlock global mutex

    mutex_unlock(&kbdus_control_mutex_);

    // success

    return 0;
}

/**
 * kbdus_control_ioctl_wait_until_device_is_destroyed_() - Implements the
 *     `KBDUS_IOCTL_WAIT_UNTIL_DEVICE_IS_DESTROYED` ioctl command for the
 *     `unlocked_ioctl` and `compat_ioctl` file operations of the control
 *     device.
 * @seqnum_usrptr: TODO.
 *
 * Return: TODO.
 */
static int kbdus_control_ioctl_wait_until_device_is_destroyed_(
    const u64 __user *seqnum_usrptr
    )
{
    u64 seqnum;
    int ret;

    // copy device seqnum from user space

    if (get_user(seqnum, seqnum_usrptr) != 0)
        return -EFAULT;

    // lock wait queue spin lock

    spin_lock(&kbdus_control_device_destroyed_.lock);

    // check if seqnum was ever used

    if (seqnum >= kbdus_control_next_seqnum_)
    {
        // device with given seqnum never existed

        ret = -EINVAL;
    }
    else
    {
        // wait for device to be destroyed (if not already)

        ret = wait_event_interruptible_locked(
            kbdus_control_device_destroyed_,
            !kbdus_control_lookup_device_wrapper_by_seqnum_(seqnum)
            );
    }

    // unlock wait queue spin lock

    spin_unlock(&kbdus_control_device_destroyed_.lock);

    // return result

    return ret;
}

/**
 * kbdus_control_ioctl_unknown_() - Implements the handling of unknown (but
 *     possibly known to the session's protocol instance) ioctl command for the
 *     `unlocked_ioctl` and `compat_ioctl` file operations of the control
 *     device.
 * @session: TODO.
 * @command: TODO.
 * @argument_usrptr: TODO.
 *
 * Return: TODO.
 */
static int kbdus_control_ioctl_unknown_(
    struct kbdus_control_session_ *session,
    unsigned int command,
    void __user *argument_usrptr
    )
{
    // ensure that session is associated with a device

    if (!test_bit(KBDUS_CONTROL_SESSION_FLAG_ATTACHED_, &session->flags))
        return -ENOTTY;

    // delegate ioctl to device

    return kbdus_device_handle_control_ioctl(
        session->device_wrapper->device,
        session->protocol,
        session->protocol_instance,
        command,
        argument_usrptr
        );
}

/**
 * kbdus_control_ioctl_() - Implements the `unlocked_ioctl` and `compat_ioctl`
 *     file operations of the control device.
 * @filp: TODO.
 * @command: TODO.
 * @argument: TODO.
 *
 * Return: TODO.
 */
static long kbdus_control_ioctl_(
    struct file *filp,
    unsigned int command,
    unsigned long argument
    )
{
    struct kbdus_control_session_ *session;
    void __user *argument_usrptr;

    session = filp->private_data;
    argument_usrptr = (void __user *)argument;

    // process command

    switch (command)
    {
    case KBDUS_IOCTL_GET_VERSION:
        return kbdus_control_ioctl_get_version_(argument_usrptr);

    case KBDUS_IOCTL_CREATE_DEVICE:
        return kbdus_control_ioctl_create_device_(session, argument_usrptr);

    case KBDUS_IOCTL_ATTACH_TO_DEVICE:
        return kbdus_control_ioctl_attach_to_device_(session, argument_usrptr);

    case KBDUS_IOCTL_MARK_AS_SUCCESSFUL:
        return kbdus_control_ioctl_mark_as_successful_(session);

    case KBDUS_IOCTL_DEVICE_INDEX_TO_SEQNUM:
        return kbdus_control_ioctl_device_index_to_seqnum_(argument_usrptr);

    case KBDUS_IOCTL_GET_DEVICE_CONFIG:
        return kbdus_control_ioctl_get_device_config_(argument_usrptr);

    case KBDUS_IOCTL_FLUSH_DEVICE:
        return kbdus_control_ioctl_flush_device_(argument_usrptr);

    case KBDUS_IOCTL_REQUEST_SESSION_TERMINATION:
        return kbdus_control_ioctl_request_session_termination_(
            argument_usrptr
            );

    case KBDUS_IOCTL_REQUEST_DEVICE_DESTRUCTION:
        return kbdus_control_ioctl_request_device_destruction_(argument_usrptr);

    case KBDUS_IOCTL_WAIT_UNTIL_DEVICE_IS_DESTROYED:
        return kbdus_control_ioctl_wait_until_device_is_destroyed_(
            argument_usrptr
            );

    default:
        return kbdus_control_ioctl_unknown_(session, command, argument_usrptr);
    }
}

// File operations of the control device.
static const struct file_operations kbdus_control_ops_ =
{
    .owner          = THIS_MODULE,

    .open           = kbdus_control_open_,
    .release        = kbdus_control_release_,

    .llseek         = no_llseek,
    .read_iter      = kbdus_control_read_iter_,
    .write_iter     = kbdus_control_write_iter_,

    .mmap           = kbdus_control_mmap_,

    .unlocked_ioctl = kbdus_control_ioctl_,
    .compat_ioctl   = kbdus_control_ioctl_,
};

/* -------------------------------------------------------------------------- */
/* component init / exit */

int __init kbdus_control_init(void)
{
    int ret;

    // validate module parameter 'max_devices'

    if ((ret = validate_module_parameters_()) != 0)
        goto error;

    // register control

    ret = alloc_chrdev_region(
        &kbdus_control_majorminor_, 0, 1, KBDUS_CONTROL_DEVICE_NAME_
        );

    if (ret < 0)
        goto error;

    // initialize and add control device

    cdev_init(&kbdus_control_cdev_, &kbdus_control_ops_);

    ret = cdev_add(&kbdus_control_cdev_, kbdus_control_majorminor_, 1);

    if (ret < 0)
        goto error_unregister_chrdev_region;

    // create class for control device

    kbdus_control_class_ = class_create(THIS_MODULE, KBDUS_CONTROL_CLASS_NAME_);

    if (IS_ERR(kbdus_control_class_))
    {
        ret = PTR_ERR(kbdus_control_class_);
        goto error_cdev_del;
    }

    // register control device with sysfs

    kbdus_control_device_ = device_create(
        kbdus_control_class_, NULL, kbdus_control_majorminor_, NULL,
        KBDUS_CONTROL_DEVICE_NAME_
        );

    if (IS_ERR(kbdus_control_device_))
    {
        ret = PTR_ERR(kbdus_control_device_);
        goto error_class_destroy;
    }

    // success

    return 0;

    // failure

error_class_destroy:
    class_destroy(kbdus_control_class_);
error_cdev_del:
    cdev_del(&kbdus_control_cdev_);
error_unregister_chrdev_region:
    unregister_chrdev_region(kbdus_control_majorminor_, 1);
error:
    return ret;
}

void kbdus_control_exit(void)
{
    struct kbdus_control_device_wrapper_ *device_wrapper;
    struct kbdus_control_device_wrapper_ *next_device_wrapper;

    // destroy remaining devices

    list_for_each_entry_safe(
        device_wrapper, next_device_wrapper, &kbdus_control_devices_, list
        )
    {
        kbdus_assert(!device_wrapper->session);

        kbdus_device_destroy(device_wrapper->device);
        kfree(device_wrapper);
    }

    // perform some sanity checks

    kbdus_assert(!mutex_is_locked(&kbdus_control_mutex_));
    kbdus_assert(!spin_is_locked(&kbdus_control_device_destroyed_.lock));
    kbdus_assert(!waitqueue_active(&kbdus_control_device_destroyed_));

    // undo all initialization

    device_destroy(kbdus_control_class_, kbdus_control_majorminor_);
    class_destroy(kbdus_control_class_);
    cdev_del(&kbdus_control_cdev_);
    unregister_chrdev_region(kbdus_control_majorminor_, 1);
}

/* -------------------------------------------------------------------------- */
