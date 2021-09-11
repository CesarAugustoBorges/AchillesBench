#ifndef KBDUS_HEADER_KBDUS_H_
#define KBDUS_HEADER_KBDUS_H_

/* -------------------------------------------------------------------------- */
/* includes */

#if __KERNEL__
    #include <linux/ioctl.h>
    #include <linux/types.h>
#else
    #include <stdint.h>
    #include <sys/ioctl.h>
#endif

/* -------------------------------------------------------------------------- */
/* protocol-agnostic interface */

/** \brief A version number. */
struct kbdus_version
{
    /** \brief The *major* version. */
    uint32_t major;

    /** \brief The *minor* version. */
    uint32_t minor;

    /** \brief The *patch* version. */
    uint32_t patch;
};

/** \brief The configuration of a BDUS device. */
struct kbdus_device_config
{
    /**
     * \brief The device's sequence number, unique for every device since the
     *        kbdus module was loaded.
     *
     * Directionality:
     *
     * - When creating new device: OUT;
     * - When attaching to existing device: IN.
     */
    uint64_t seqnum;

    /**
     * \brief The size of the device, in bytes.
     *
     * Must be a positive multiple of `physical_block_size`, or of
     * `logical_block_size` if the former is 0.
     *
     * Directionality:
     *
     * - When creating new device: IN;
     * - When attaching to existing device: OUT.
     */
    uint64_t size;

    /**
     * \brief The device's index, which also determines the name of the
     * block special file under /dev (index = 123 --> path = /dev/bdus-123)
     *
     * Directionality:
     *
     * - When creating new device: OUT;
     * - When attaching to existing device: OUT.
     */
    uint32_t index;

    /**
     * \brief The block device's major number.
     *
     * Directionality:
     *
     * - When creating new device: OUT;
     * - When attaching to existing device: OUT.
     */
    uint32_t major;

    /**
     * \brief The block device's first minor number.
     *
     * Directionality:
     *
     * - When creating new device: OUT;
     * - When attaching to existing device: OUT.
     */
    uint32_t first_minor;

    /**
     * \brief The device's logical block size, in bytes.
     *
     * Must be a power of two greater than or equal to 512 and less than or
     * equal to the system's page size.
     *
     * Directionality:
     *
     * - When creating new device: IN;
     * - When attaching to existing device: OUT.
     */
    uint32_t logical_block_size;

    /**
     * \brief The device's physical block size, in bytes.
     *
     * Must be 0 or a power of two greater than or equal to `logical_block_size`
     * and less than or equal to the system's page size.
     *
     * - If this value is 0, it is set to the value of `logical_block_size`.
     * - Otherwise, this value is left unchanged.
     *
     * Directionality:
     *
     * - When creating new device: IN & OUT;
     * - When attaching to existing device: OUT.
     */
    uint32_t physical_block_size;

    /**
     * \brief The maximum size for *read*, *write*, and *FUA write*
     *        requests sent to the driver, in bytes.
     *
     * Must be 0 or a value greater than or equal to the system's page size.
     *
     * - If `supports_read`, `supports_write`, and `supports_fua_write` are all
     *   false, this value is set to 0;
     * - Otherwise, if this value is 0, it is set to an unspecified positive
     *   multiple of `logical_block_size` that is greater than or equal to the
     *   system's page size;
     * - Otherwise, this value is either left unchanged or decreased to an
     *   unspecified positive multiple of `logical_block_size` that is greater
     *   than or equal to the system's page size (but never increased).
     *
     * Directionality:
     *
     * - When creating new device: IN & OUT;
     * - When attaching to existing device: OUT.
     */
    uint32_t max_read_write_size;

    /**
     * \brief The maximum size for *write same* requests sent to the
     *        driver, in bytes.
     *
     * Must be 0 or a value greater than or equal to `logical_block_size`.
     *
     * - If `supports_write_same` is false, this value is set to 0;
     * - Otherwise, if this value is 0, it is set to an unspecified positive
     *   multiple of `logical_block_size`;
     * - Otherwise, this value is either left unchanged or decreased to an
     *   unspecified positive multiple of `logical_block_size` (but never
     *   increased).
     *
     * Directionality:
     *
     * - When creating new device: IN & OUT;
     * - When attaching to existing device: OUT.
     */
    uint32_t max_write_same_size;

    /**
     * \brief The maximum size for *write zeros* requests sent to the
     *        driver, in bytes.
     *
     * Must be 0 or a value greater than or equal to `logical_block_size`.
     *
     * - If `supports_write_zeros` is false, this value is set to 0;
     * - Otherwise, if this value is 0, it is set to an unspecified positive
     *   multiple of `logical_block_size`;
     * - Otherwise, this value is either left unchanged or decreased to an
     *   unspecified positive multiple of `logical_block_size` (but never
     *   increased).
     *
     * Directionality:
     *
     * - When creating new device: IN & OUT;
     * - When attaching to existing device: OUT.
     */
    uint32_t max_write_zeros_size;

    /**
     * \brief The maximum size for *discard* and *secure erase* requests sent to
     *        the driver, in bytes.
     *
     * Must be 0 or a value greater than or equal to `logical_block_size`.
     *
     * - If `supports_discard` and `supports_secure_erase` are both false, this
     *   value is set to 0;
     * - Otherwise, if this value is 0, it is set to an unspecified positive
     *   multiple of `logical_block_size`;
     * - Otherwise, this value is either left unchanged or decreased to an
     *   unspecified positive multiple of `logical_block_size` (but never
     *   increased).
     *
     * Directionality:
     *
     * - When creating new device: IN & OUT;
     * - When attaching to existing device: OUT.
     */
    uint32_t max_discard_erase_size;

    /**
     * \brief Whether the device supports *read* requests.
     *
     * Directionality:
     *
     * - When creating new device: IN;
     * - When attaching to existing device: OUT.
     */
    uint8_t supports_read;

    /**
     * \brief Whether the device supports *write* requests.
     *
     * Directionality:
     *
     * - When creating new device: IN;
     * - When attaching to existing device: OUT.
     */
    uint8_t supports_write;

    /**
     * \brief Whether the device supports *write same* requests.
     *
     * Directionality:
     *
     * - When creating new device: IN;
     * - When attaching to existing device: OUT.
     */
    uint8_t supports_write_same;

    /**
     * \brief Whether the device supports *write zeros* requests (both allowing
     * and disallowing unmapping).
     *
     * Directionality:
     *
     * - When creating new device: IN;
     * - When attaching to existing device: OUT.
     */
    uint8_t supports_write_zeros;

    /**
     * \brief Whether the device supports *FUA write* requests.
     *
     * Directionality:
     *
     * - When creating new device: IN;
     * - When attaching to existing device: OUT.
     */
    uint8_t supports_fua_write;

    /**
     * \brief Whether the device supports *flush* requests.
     *
     * Directionality:
     *
     * - When creating new device: IN;
     * - When attaching to existing device: OUT.
     */
    uint8_t supports_flush;

    /**
     * \brief Whether the device supports *discard* requests.
     *
     * Directionality:
     *
     * - When creating new device: IN;
     * - When attaching to existing device: OUT.
     */
    uint8_t supports_discard;

    /**
     * \brief Whether the device supports *secure erase* requests.
     *
     * Directionality:
     *
     * - When creating new device: IN;
     * - When attaching to existing device: OUT.
     */
    uint8_t supports_secure_erase;

    /**
     * \brief Whether the device supports *ioctl* requests.
     *
     * Directionality:
     *
     * - When creating new device: IN;
     * - When attaching to existing device: OUT.
     */
    uint8_t supports_ioctl;

    /**
     * \brief TODO: document (inverse of req queue flag `QUEUE_FLAG_NONROT`)
     *
     * Even if the device is not rotational, it may make sense to set this to
     * true as this may result in fewer, larger requests being transmitted to
     * the user-space driver, possibly improving performance.
     *
     * Directionality:
     *
     * - When creating new device: IN;
     * - When attaching to existing device: OUT.
     */
    uint8_t is_rotational;

    /**
     * \brief TODO: document (tag set flag `BLK_MQ_F_SHOULD_MERGE`)
     *
     * Directionality:
     *
     * - When creating new device: IN;
     * - When attaching to existing device: OUT.
     */
    uint8_t should_merge_requests;

    /**
     * \brief TODO: document
     *
     * If non-zero, each request is sent to the process (or thread) that
     * requested it last (LIFO order), instead of the one that has been waiting
     * for the most time (FIFO order).
     *
     * This can in principle reduce the number of context switches by giving the
     * next request to the process that just asked for it. However, no evidence
     * of any resulting performance difference has been found so far.
     *
     * Directionality:
     *
     * - When creating new device: IN;
     * - When attaching to existing device: OUT.
     */
    uint8_t lifo_request_transmission;

    /**
     * \brief The maximum amount of *active* queue requests.
     *
     * An active queue request is one that has been delegated to a protocol
     * through `process_queue_request()` but not yet completed.
     *
     * Effectively, this is the maximum amount of queue requests that a protocol
     * can "see" at once for a device.
     *
     * Directionality:
     *
     * - When creating new device: IN;
     * - When attaching to existing device: OUT.
     */
    uint32_t max_active_queue_reqs;

    /**
     * \brief The maximum amount of *active* ioctl requests.
     *
     * An active ioctl request is one that has been delegated to a protocol
     * through `process_ioctl_request()` but not yet completed (which happens
     * when that function returns).
     *
     * Note that this value does *not* limit the amount of concurrent
     * invocations of the `ioctl()` system call on a device -- when this limit
     * is reached, calls block until they can be delegated to the protocol.
     *
     * Equivalently, this value is the maximum amount of concurrent invocations
     * of the `process_ioctl` callback in `struct kbdus_protocol` for each
     * device.
     *
     * Effectively, this value is the maximum amount of ioctl requests that a
     * protocol can "see" at once for a device.
     *
     * Directionality:
     *
     * - When creating new device: IN;
     * - When attaching to existing device: OUT.
     */
    uint32_t max_active_ioctl_reqs;

    /**
     * \brief Whether to disable partition scanning for the device.
     *
     * Directionality:
     *
     * - When creating new device: IN;
     * - When attaching to existing device: OUT.
     */
    uint8_t disable_partition_scanning;

    /**
     * \brief TODO: document
     *
     * Note that the device is always destroyed if the original driver detaches
     * before the device becomes available to users.
     *
     * Note that if the driver fails before the device becomes available to
     * users, the device will be destroyed regardless of this value.
     *
     * Directionality:
     *
     * - When creating new device: IN;
     * - When attaching to existing device: OUT.
     */
    uint8_t destroy_when_detached;

    /** \cond PRIVATE */
    uint8_t padding_[6];
    /** \endcond */
};

/** \brief TODO: document */
struct kbdus_protocol_config
{
    /**
     * \brief The name of the protocol to be used.
     *
     * Directionality:
     *
     * - When creating new device: IN;
     * - When attaching to existing device: IN.
     */
    char name[32];

    /**
     * \brief TODO: document
     *
     * Only applies to the "mmap" protocol.
     *
     * Directionality:
     *
     * - When creating new device: IN;
     * - When attaching to existing device: IN.
     */
    uint32_t num_request_buffers;

    /** \cond PRIVATE */
    uint32_t padding_;
    /** \endcond */
};

/** \brief TODO: document */
struct kbdus_config
{
    /** \brief TODO: document */
    struct kbdus_device_config device;

    /** \brief TODO: document */
    struct kbdus_protocol_config protocol;
};

/** \brief TODO: document */
enum kbdus_request_type
{
    /**
     * \brief Indicates that the device has become available to users.
     *
     * Requests of this type belong to the category of "notification" requests
     * and should not be replied to.
     */
    KBDUS_REQUEST_TYPE_DEVICE_AVAILABLE,

    /**
     * \brief Sent when driver should terminate.
     *
     * Requests of this type belong to the category of "notification" requests
     * and should not be replied to.
     */
    KBDUS_REQUEST_TYPE_TERMINATE,

    /**
     * \brief TODO: document
     *
     * Requests of this type belong to the category of "notification" requests
     * and should not be replied to.
     */
    KBDUS_REQUEST_TYPE_FLUSH_AND_TERMINATE,

    /**
     * \brief *Read* request.
     *
     * - 64-bit argument: read offset;
     * - 32-bit argument: number of bytes to be read;
     * - Request payload: (none);
     * - Reply payload: the read data, if the operation was successful;
     * - Reply payload size: the 32-bit argument.
     *
     * Requests of this type belong to the category of "queue" requests.
     */
    KBDUS_REQUEST_TYPE_READ,

    /**
     * \brief *Write* request.
     *
     * - 64-bit argument: write offset;
     * - 32-bit argument: number of bytes to be written;
     * - Request payload: the data to be written;
     * - Request payload size: the 32-bit argument;
     * - Reply payload: (none).
     *
     * Requests of this type belong to the category of "queue" requests.
     */
    KBDUS_REQUEST_TYPE_WRITE,

    /**
     * \brief *Write same* request.
     *
     * - 64-bit argument: write offset;
     * - 32-bit argument: number of bytes to be written;
     * - Request payload: the data to be written;
     * - Request payload size: the device's logical block size;
     * - Reply payload: (none).
     *
     * Requests of this type belong to the category of "queue" requests.
     */
    KBDUS_REQUEST_TYPE_WRITE_SAME,

    /**
     * \brief *Write zeros* request which *must not* deallocate space.
     *
     * - 64-bit argument: write offset;
     * - 32-bit argument: number of bytes to be zeroed;
     * - Request payload: (none);
     * - Reply payload: (none).
     *
     * Requests of this type belong to the category of "queue" requests.
     */
    KBDUS_REQUEST_TYPE_WRITE_ZEROS_NO_UNMAP,

    /**
     * \brief *Write zeros* request which *may* deallocate space.
     *
     * - 64-bit argument: write offset;
     * - 32-bit argument: number of bytes to be zeroed;
     * - Request payload: (none);
     * - Reply payload: (none).
     *
     * Requests of this type belong to the category of "queue" requests.
     */
    KBDUS_REQUEST_TYPE_WRITE_ZEROS_MAY_UNMAP,

    /**
     * \brief *FUA write* request.
     *
     * - 64-bit argument: write offset;
     * - 32-bit argument: number of bytes to be written;
     * - Request payload: the data to be written;
     * - Request payload size: the 32-bit argument;
     * - Reply payload: (none).
     *
     * Requests of this type belong to the category of "queue" requests.
     */
    KBDUS_REQUEST_TYPE_FUA_WRITE,

    /**
     * \brief *Flush* request.
     *
     * - 64-bit argument: (unused);
     * - 32-bit argument: (unused);
     * - Request payload: (none);
     * - Reply payload: (none).
     *
     * Requests of this type belong to the category of "queue" requests.
     */
    KBDUS_REQUEST_TYPE_FLUSH,

    /**
     * \brief *Discard* request.
     *
     * - 64-bit argument: discard offset;
     * - 32-bit argument: number of bytes to be discarded;
     * - Request payload: (none);
     * - Reply payload: (none).
     *
     * Requests of this type belong to the category of "queue" requests.
     */
    KBDUS_REQUEST_TYPE_DISCARD,

    /**
     * \brief *Secure erase* request.
     *
     * - 64-bit argument: secure erase offset;
     * - 32-bit argument: number of bytes to be securely erased;
     * - Request payload: (none);
     * - Reply payload: (none).
     *
     * Requests of this type belong to the category of "queue" requests.
     */
    KBDUS_REQUEST_TYPE_SECURE_ERASE,

    /**
     * \brief *ioctl* request.
     *
     * - 64-bit argument: (unused);
     * - 32-bit argument: ioctl command;
     * - Request payload: the argument data, if the ioctl command's direction
     *   has _IOC_READ;
     * - Request payload size: given by the ioctl command, if the ioctl
     *   command's direction has _IOC_READ;
     * - Reply payload: the argument data, if the ioctl command's direction has
     *   _IOC_WRITE;
     * - Reply payload size: given by the ioctl command, if the ioctl command's
     *   direction has _IOC_WRITE.
     *
     * Requests of this type belong to the category of "queue" requests.
     */
    KBDUS_REQUEST_TYPE_IOCTL,
};

/** \brief The "type" of all `ioctl` commands known to the control device. */
#define KBDUS_IOCTL_TYPE 0xBD

/**
 * \brief Returns kbdus' version.
 *
 * - Fails with errno = EFAULT if memory copy to user space fails.
 * - Returns 0 on success.
 */
#define KBDUS_IOCTL_GET_VERSION \
    _IOW(KBDUS_IOCTL_TYPE, 0, struct kbdus_version)

/**
 * \brief Creates a device and attaches the file description to that device.
 *
 * If the file description fails to attach to the device, then the device is
 * unconditionally destroyed before the ioctl returns.
 *
 * The given configuration is adjusted as specified in the documentation for
 * `struct kbdus_dev_config` and `struct kbdus_protocol_config`.
 *
 * Note that a device index might be occupied even if the corresponding block
 * special file does not exist in `/dev`, because there might still be open file
 * descriptions to it, and these half-dead currently count against the maximum
 * number of simultaneously existing BDUS devices (but only if the kernel is
 * configured with debugfs enabled and on kernel 4.11 or higher, which is a
 * weird condition, I know). (TODO: enforce this always)
 *
 * If this ioctl fails, its argument is left in an unspecified state.
 *
 * - Fails with errno = EFAULT if memory copy from/to user space fails.
 * - Fails with errno = EINVAL if the file description also is attaching or
 *   attached to a device.
 * - Fails with errno = EINVAL if the given configuration is invalid.
 * - Fails with errno = ENOSPC if the maximum number of BDUS devices exists.
 * - May fail due to other reasons with other errno values as well.
 * - Returns 0 on success.
 */
#define KBDUS_IOCTL_CREATE_DEVICE \
    _IOWR(KBDUS_IOCTL_TYPE, 1, struct kbdus_config)

/**
 * \brief Attach the file description to a given device.
 *
 * The seqnum of the device is taken from the `device.seqnum` field of the
 * configuration given as the ioctl argument. Other fields of `device` are not
 * inspected, and the `device` is overwritten with the existing device's
 * configuration. *The configuration of the existing device is not modified in
 * any way.*
 *
 * The `protocol` field of the configuration given as the ioctl argument can be
 * completely different of that of file descriptions previously attached to the
 * existing device (*i.e.*, a different protocol with different configurations
 * can be used).
 *
 * The file description must not be attached (or attaching) to any device.
 *
 * A file description can only be attached to devices that are already available
 * to clients. Note, however, that the first request received through the file
 * description's protocol is always a "device available" notification.
 *
 * The file description can be attached to a device without any attached file
 * description. It can also be attached to a device which already has an
 * attached file description, in which case the already-attached file
 * description is prompted to terminate and then the new file description
 * attached to the device. However, if the new file description fails to attach
 * to the device, the device will be left without an attached file description.
 * If the device is not allowed to exist in this state, it is destroyed.
 *
 * If this ioctl fails, its argument is left in an unspecified state.
 *
 * - Fails with errno = EFAULT if memory copy from/to user space fails.
 * - Fails with errno = EINVAL if the file descriptor also has a device
 *   associated with it.
 * - Fails with errno = EINVAL if no device with the given seqnum ever existed.
 * - Fails with errno = ENODEV if the device with the given seqnum doesn't exist
 *   anymore.
 * - Fails with errno = EBUSY if the device is not yet available to clients.
 * - Fails with errno = EINPROGRESS if someone else is trying to attach to the
 *   device.
 * - Fails with errno = EINTR if interrupted while waiting for existing driver
 *   to terminate. Note that the driver can then be destroyed if it can't exist
 *   without a controlling driver and you are too slow to retry this ioctl.
 *   (TODO: we should really find some clean way of avoiding this!)
 * - May fail due to other reasons with other errno values as well.
 * - Returns 0 on success.
 */
#define KBDUS_IOCTL_ATTACH_TO_DEVICE \
    _IOWR(KBDUS_IOCTL_TYPE, 2, struct kbdus_config)

/**
 * \brief TODO: document
 *
 * Marks *the current session* as successful.
 *
 * If a session is attached to a device and it is closed without being marked as
 * successful, the device will be destroyed if it is not allowed to exist after
 * detach, even if some other session is trying to attach to it.
 *
 * Always succeeds.
 */
#define KBDUS_IOCTL_MARK_AS_SUCCESSFUL \
    _IO(KBDUS_IOCTL_TYPE, 3)

/**
 * \brief Takes a device index and overwrites it with the seqnum of the
 *        corresponding device.
 *
 * If this ioctl fails, its argument is left in an unspecified state.
 *
 * - Fails with errno = EFAULT if memory copy from/to user space fails.
 * - Fails with errno = EINVAL if the given index is invalid (because it is too
 *   big).
 * - Fails with errno = ENODEV if no device with the given index exists.
 * - Returns 0 on success.
 */
#define KBDUS_IOCTL_DEVICE_INDEX_TO_SEQNUM \
    _IOWR(KBDUS_IOCTL_TYPE, 4, uint64_t)

/**
 * \brief TODO: document
 *
 * If this ioctl fails, its argument is left in an unspecified state.
 *
 * - Fails with errno = EFAULT if memory copy from/to user space fails.
 * - Fails with errno = EINVAL if no device with the given seqnum ever existed.
 * - Fails with errno = ENODEV if the device with the given seqnum doesn't exist
 *   anymore.
 * - Fails with errno = EINTR if interrupted.
 */
#define KBDUS_IOCTL_GET_DEVICE_CONFIG \
    _IOWR(KBDUS_IOCTL_TYPE, 5, struct kbdus_device_config)

/**
 * \brief Ensures that all data written to the device with the given seqnum is
 *        persistently stored, as if fsync() or fdatasync() were called on it.
 *
 * - Blocks until it can ensure that.
 * - Fails with errno = EFAULT if memory copy from user space fails.
 * - Fails with errno = EINVAL if no device with the given seqnum ever existed.
 * - Fails with errno = ENODEV if the device with the given seqnum doesn't exist
 *   anymore.
 * - May fail due to other reasons with other errno values as well.
 * - Returns 0 on success.
 */
#define KBDUS_IOCTL_FLUSH_DEVICE \
    _IOR(KBDUS_IOCTL_TYPE, 6, uint64_t)

/**
 * \brief TODO: document
 *
 * Terminates the driver of the device with the given seqnum.
 *
 * May return before the device is actually terminated.
 *
 * If the device is not allowed to exist without a session attached to it, then
 * it will be destroyed. Otherwise it will just be left without a driver.
 *
 * Note that if the target device is not yet available to users, it will be
 * unconditionally destroyed.
 *
 * If the device already has no attached file description, nothing is done and
 * the ioctl succeeds.
 *
 * - Fails with errno = EFAULT if memory copy from user space fails.
 * - Fails with errno = EINVAL if no device with the given seqnum ever existed.
 * - Fails with errno = ENODEV if the device with the given seqnum doesn't exist
 *   anymore.
 */
#define KBDUS_IOCTL_REQUEST_SESSION_TERMINATION \
    _IOR(KBDUS_IOCTL_TYPE, 7, uint64_t)

/**
 * \brief Request that the device with the given seqnum be destroyed.
 *
 * Pending and future requests to the device fail.
 *
 * May return before the device is actually terminated.
 *
 * If the device was already destroyed, nothing is done and the ioctl succeeds.
 *
 * - Returns 0 on success.
 * -
 * - Fails with errno = EFAULT if memory copy from user space fails.
 * - Fails with errno = EINVAL if no device with the given seqnum ever existed.
 * - May fail due to other reasons with other errno values as well.
 */
#define KBDUS_IOCTL_REQUEST_DEVICE_DESTRUCTION \
    _IOR(KBDUS_IOCTL_TYPE, 8, uint64_t)

/**
 * \brief Blocks until the device with the given seqnum is destroyed.
 *
 * If the device was already destroyed, succeeds immediately.
 *
 * - Returns 0 on success.
 * - Fails with errno = EFAULT if memory copy from user space fails.
 * - Fails with errno = EINVAL if no device with the given seqnum ever existed.
 * - May fail due to other reasons with other errno values as well.
 */
#define KBDUS_IOCTL_WAIT_UNTIL_DEVICE_IS_DESTROYED \
    _IOR(KBDUS_IOCTL_TYPE, 9, uint64_t)

/* -------------------------------------------------------------------------- */
/* interface for protocol "ioctl" */

/** \brief TODO: document */
struct kbdusioctl_request
{
    /** \brief TODO: document */
    uint64_t buffer_ptr;

    /** \brief TODO: document */
    uint64_t handle_seqnum;

    /** \brief TODO: document */
    uint16_t handle_index;

    /** \brief [OUT] Request type. */
    uint16_t type;

    /** \brief [OUT] 32-bit request argument (if applicable). */
    uint32_t arg32;

    /** \brief [OUT] 64-bit request argument (if applicable). */
    uint64_t arg64;
};

/** \brief TODO: document */
struct kbdusioctl_reply
{
    /**
     * \brief [IN pointer, points to IN/OUT buffer] Pointer to buffer with data
     * of the reply (if this communication has a reply) and which will then be
     * used to store the data of the next received request (*write*, *write
     * same*, *write zero*, or *FUA write* request, or *ioctl* argument data).
     * large enough to hold the data for any possible request. Buffer must have
     * a size at least as big as the requested read size, unless error != 0, in
     * which case this field is not inspected.
     */
    uint64_t buffer_ptr;

    /**
     * \brief [IN/OUT] If 0, this communication has no reply. Otherwise, it has
     * a reply for the request with this handle, with the data in `buffer_ptr`
     * (if applicable) and with the error in `error`.
     */
    uint64_t handle_seqnum;

    /** \brief TODO: document */
    uint16_t handle_index;

    /** \cond PRIVATE */
    uint16_t padding_;
    /** \endcond */

    /** \brief [IN] 0 if operation succeeded, negated errno value otherwise. */
    int32_t negated_errno;
};

/** \brief TODO: document */
union kbdusioctl_reply_and_request
{
    /** \brief TODO: document */
    struct kbdusioctl_reply reply;

    /** \brief TODO: document */
    struct kbdusioctl_request request;

    /**
     * \brief Aliases the `buffer_ptr`, `handle_seqnum`, and `handle_index`
     *        fields common to `reply` and `request`.
     */
    struct
    {
        /**
         * \brief An alias for the `reply.buffer_ptr` and `request.buffer_ptr`
         *        fields.
         */
        uint64_t buffer_ptr;

        /**
         * \brief An alias for the `reply.handle_seqnum` and
         *        `request.handle_seqnum` fields.
         */
        uint64_t handle_seqnum;

        /**
         * \brief An alias for the `reply.handle_index` and
         *        `request.handle_index` fields.
         */
        uint16_t handle_index;
    } common;
};

/** \brief TODO: document */
#define KBDUSIOCTL_IOCTL_RECEIVE_REQUEST \
    _IOWR(KBDUS_IOCTL_TYPE, 10, struct kbdusioctl_request)

/** \brief TODO: document */
#define KBDUSIOCTL_IOCTL_SEND_REPLY \
    _IOR(KBDUS_IOCTL_TYPE, 11, struct kbdusioctl_reply)

/**
 * Sends a reply (if `handle` in is not 0) and then receives a request. If no
 * more requests are available and will never become available (i.e., the device
 * is done and the driver should terminate), a `KBDUS_REQUEST_TYPE_TERMINATE`
 * request is received. This may only fail with `EINTR` after the reply was
 * successfully sent, so when retrying the `handle` might first be set to 0, but
 * it's not necessary since attempts to complete an already completed request
 * are ignored.
 *
 * If the device has been terminated, this just sends infinite "terminate"
 * requests.
 */
#define KBDUSIOCTL_IOCTL_SEND_REPLY_AND_RECEIVE_REQUEST \
    _IOWR(KBDUS_IOCTL_TYPE, 12, union kbdusioctl_reply_and_request)

/* -------------------------------------------------------------------------- */
/* interface for protocol "mmap" */

/** \brief TODO: document */
struct kbdusmmap_request
{
    /** \brief TODO: document */
    uint64_t handle_seqnum;

    /** \brief TODO: document */
    uint16_t handle_index;

    /** \brief [OUT] Request type. */
    uint16_t type;

    /** \brief [OUT] 32-bit request argument (if applicable). */
    uint32_t arg32;

    /** \brief [OUT] 64-bit request argument (if applicable). */
    uint64_t arg64;
};

/** \brief TODO: document */
struct kbdusmmap_reply
{
    /** \brief TODO: document */
    uint64_t handle_seqnum;

    /** \brief TODO: document */
    uint16_t handle_index;

    /** \cond PRIVATE */
    uint16_t padding_;
    /** \endcond */

    /** \brief [IN] 0 if operation succeeded, negated errno value otherwise. */
    int32_t negated_errno;
};

/** \brief TODO: document */
union kbdusmmap_reply_and_request
{
    /** \brief TODO: document */
    struct kbdusmmap_reply reply;

    /** \brief TODO: document */
    struct kbdusmmap_request request;

    /**
     * \brief Aliases the `handle_seqnum` and `handle_index` fields common to
     *        `reply` and `request`.
     */
    struct
    {
        /**
         * \brief An alias for the `reply.handle_seqnum` and
         *        `request.handle_seqnum` fields.
         */
        uint64_t handle_seqnum;

        /**
         * \brief An alias for the `reply.handle_index` and
         *        `request.handle_index` fields.
         */
        uint16_t handle_index;
    } common;
};

/** \brief TODO: document */
#define KBDUSMMAP_IOCTL_RECEIVE_REQUEST \
    _IO(KBDUS_IOCTL_TYPE, 13)

/** \brief TODO: document */
#define KBDUSMMAP_IOCTL_SEND_REPLY \
    _IO(KBDUS_IOCTL_TYPE, 14)

/**
 * \brief TODO: document
 *
 * Argument is not a pointer, just a number giving the index of the request
 * buffer to use.
 */
#define KBDUSMMAP_IOCTL_SEND_REPLY_AND_RECEIVE_REQUEST \
    _IO(KBDUS_IOCTL_TYPE, 15)

/* -------------------------------------------------------------------------- */
/* interface for protocol "rw" */

/**
 * \brief The header of a request. If applicable, a payload follows the header.
 */
struct kbdusrw_request_header
{
    /** \brief [OUT] TODO: document */
    uint64_t handle_seqnum;

    /** \brief [OUT] TODO: document */
    uint16_t handle_index;

    /** \brief [OUT] The request's type. */
    uint16_t type;

    /** \brief [OUT] The request's 32-bit argument (if applicable). */
    uint32_t arg32;

    /** \brief [OUT] The request's 64-bit argument (if applicable). */
    uint64_t arg64;
};

/**
 * \brief The header of a reply, which corresponds to a previously received
 *        request. If applicable, a payload follows the header.
 */
struct kbdusrw_reply_header
{
    /** \brief [IN] TODO: document */
    uint64_t handle_seqnum;

    /** \brief [IN] TODO: document */
    uint16_t handle_index;

    /** \cond PRIVATE */
    uint16_t padding_;
    /** \endcond */

    /** \brief [IN] TODO: document */
    int32_t negated_errno;
};

/* -------------------------------------------------------------------------- */

#endif /* KBDUS_HEADER_KBDUS_H_ */
