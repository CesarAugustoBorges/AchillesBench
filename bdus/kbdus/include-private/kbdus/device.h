#ifndef KBDUS_HEADER_DEVICE_H_
#define KBDUS_HEADER_DEVICE_H_

/* -------------------------------------------------------------------------- */
/* includes */

#include <kbdus.h>
#include <kbdus/protocol.h>

#include <linux/blkdev.h>
#include <linux/init.h>
#include <linux/version.h>

/* -------------------------------------------------------------------------- */
/* component init / exit */

/** TODO: document */
int kbdus_device_init(void) __init;

/** TODO: document */
void kbdus_device_exit(void);

/* -------------------------------------------------------------------------- */
/* component interface */

/** \brief Represents a BDUS block device. */
struct kbdus_device;

enum kbdus_device_state
{
    // Initial state.
    //
    // Can transition by itself to state ACTIVE.
    // Can transition to state TERMINATED by calling kbdus_device_terminate().
    KBDUS_DEVICE_STATE_UNAVAILABLE,

    // Normal state.
    //
    // Can transition to state INACTIVE by calling kbdus_device_deactivate().
    // Can transition to state TERMINATED by calling kbdus_device_terminate().
    KBDUS_DEVICE_STATE_ACTIVE,

    // Accepts requests but they are not sent to protocol instances. Protocol
    // instances only receive termination requests.
    //
    // Can transition to state ACTIVE by calling kbdus_device_activate().
    // Can transition to state TERMINATED by calling kbdus_device_terminate().
    KBDUS_DEVICE_STATE_INACTIVE,

    // Accepts requests but they are immediately failed. When transitioning to
    // this state, existing requests are also immediately failed. Protocol
    // instances only receive termination requests.
    //
    // Can not transition to any other state.
    KBDUS_DEVICE_STATE_TERMINATED,
};

/**
 * \brief Validates the given device configuration and, if it is valid, adjusts
 *        its fields.
 *
 * Note that this function also sets the `major` field of the given
 * configuration to the appropriate value.
 *
 * THREAD-SAFETY: This function may be invoked concurrently with itself and with
 * any other function exposed by this component, with the exception that
 * concurrent invocations of this function on the same configuration result in
 * undefined behavior.
 *
 * SLEEPING: This function never sleeps.
 */
int kbdus_device_validate_and_adjust_config(struct kbdus_device_config *config);

/**
 * \brief Creates a device.
 *
 * The given configuration must have been previously processed by
 * `kbdus_device_validate_and_adjust_config()`, and its `index` and
 * `first_minor` fields must have been set to appropriate values.
 *
 * Returns an `ERR_PTR()` on error.
 *
 * THREAD-SAFETY: This function may be invoked concurrently with itself and with
 * any other function exposed by this component.
 *
 * SLEEPING: This function might sleep.
 */
struct kbdus_device *kbdus_device_create(
    const struct kbdus_device_config *config
    );

/**
 * \brief Destroys a device.
 *
 * This function does not require interaction with the user-space driver, and
 * may be invoked even if `kbdus_device_request_termination()` was not.
 *
 * Note that this function causes all pending requests submitted to the device
 * to fail and does not attempt to persist previously written data.
 *
 * THREAD-SAFETY: This function may be invoked concurrently with itself and with
 * any other function exposed by this component, with the exception that this
 * function may *not* be invoked concurrently on the same device with
 * **itself**, `kbdus_device_request_termination()`,
 * `kbdus_device_handle_control_read_iter()`,
 * `kbdus_device_handle_control_write_iter()`, or
 * `kbdus_device_handle_control_ioctl()`.
 *
 * SLEEPING: This function might sleep.
 */
void kbdus_device_destroy(struct kbdus_device *device);

/**
 * \brief TODO: document
 */
enum kbdus_device_state kbdus_device_get_state(
    const struct kbdus_device *device
    );

/**
 * \brief TODO: document
 */
const struct kbdus_device_config *kbdus_device_get_config(
    const struct kbdus_device *device
    );

/**
 * \brief TODO: document
 */
bool kbdus_device_is_read_only(const struct kbdus_device *device);

/**
 * \brief Gets and returns a reference to the `block_device` corresponding to
 *        partition 0 (*i.e.*, the whole device) of the given device.
 *
 * The returned `block_device` should later be put using `bdput()` and can be
 * used even after (or during) calling `kbdus_device_destroy()` on the same
 * device.
 *
 * Returns `NULL` on error.
 *
 * THREAD-SAFETY: This function may be invoked concurrently with itself and with
 * any other function exposed by this component, with the exception that this
 * function may *not* be invoked concurrently on the same device with
 * `kbdus_device_destroy()`.
 *
 * SLEEPING: This function might sleep.
 */
struct block_device *kbdus_device_get_block_device(struct kbdus_device *device);

/**
 * \brief Request the termination of the given device.
 *
 * This function prevents the submission of new requests to the device and fails
 * already submitted requests, and instructs the driver to terminate.
 *
 * This function may be called more than once on the same device, or never at
 * all. Only the first invocation has any effect.
 *
 * Note that this function may return before the aforementioned process is fully
 * performed.
 *
 * THREAD-SAFETY: This function may be invoked concurrently with itself and with
 * any other function exposed by this component, with the exception that this
 * function may *not* be invoked concurrently on the same device with
 * `kbdus_device_destroy()`, `kbdus_device_deactivate()`, or
 * `kbdus_device_activate()`.
 *
 * SLEEPING: This function might sleep.
 */
void kbdus_device_terminate(struct kbdus_device *device);

/**
 * \brief TODO: document
 *
 * RETURNS TRUE IF THE DEVICE BECAME TERMINATED WITH THIS CALL OR WAS ALREADY
 * TERMINATED!
 *
 * If state is:
 *
 * - STARTING, then device is terminated;
 * - ACTIVE, then device becomes INACTIVE and requests given to a protocol
 *   instance but not yet completed will later be available again to a future
 *   protocol instance;
 * - INACTIVE, then nothing is done;
 * - TERMINATED, then nothing is done.
 *
 * THREAD-SAFETY: This function may be invoked concurrently with itself and with
 * any other function exposed by this component, with the exception that this
 * function may *not* be invoked concurrently on the same device with
 * `kbdus_device_destroy()`, `kbdus_device_terminate()`, or
 * `kbdus_device_activate()`.
 *
 * SLEEPING: This function never sleeps.
 */
void kbdus_device_deactivate(struct kbdus_device *device, bool flush);

/**
 * \brief TODO: document
 *
 * MAY ONLY BE CALLED IF THE CURRENT STATE IS "INACTIVE".
 *
 * THREAD-SAFETY: This function may be invoked concurrently with itself and with
 * any other function exposed by this component, with the exception that this
 * function may *not* be invoked concurrently on the same device with
 * `kbdus_device_destroy()`, `kbdus_device_terminate()`, or
 * `kbdus_device_deactivate()`.
 *
 * SLEEPING: This function never sleeps.
 */
void kbdus_device_activate(struct kbdus_device *device);

/**
 * \brief Delegates processing of the control device's `read_iter` file
 *        operation to the given device.
 *
 * THREAD-SAFETY: This function may be invoked concurrently with itself and with
 * any other function exposed by this component, with the exception that this
 * function may *not* be invoked concurrently on the same device with
 * `kbdus_device_destroy()`.
 *
 * SLEEPING: This function might sleep.
 */
ssize_t kbdus_device_handle_control_read_iter(
    struct kbdus_device *device,
    const struct kbdus_protocol *protocol,
    struct kbdus_protocol_instance *protocol_instance,
    struct iov_iter *to
    );

/**
 * \brief Delegates processing of the control device's `write_iter` file
 *        operation to the given device.
 *
 * THREAD-SAFETY: This function may be invoked concurrently with itself and with
 * any other function exposed by this component, with the exception that this
 * function may *not* be invoked concurrently on the same device with
 * `kbdus_device_destroy()`.
 *
 * SLEEPING: This function might sleep.
 */
ssize_t kbdus_device_handle_control_write_iter(
    struct kbdus_device *device,
    const struct kbdus_protocol *protocol,
    struct kbdus_protocol_instance *protocol_instance,
    struct iov_iter *from
    );

/**
 * \brief Delegates processing of the control device's `mmap` file operation to
 *        the given device.
 *
 * THREAD-SAFETY: This function may be invoked concurrently with itself and with
 * any other function exposed by this component, with the exception that this
 * function may *not* be invoked concurrently on the same device with
 * `kbdus_device_destroy()`.
 *
 * SLEEPING: This function might sleep.
 */
int kbdus_device_handle_control_mmap(
    struct kbdus_device *device,
    const struct kbdus_protocol *protocol,
    struct kbdus_protocol_instance *protocol_instance,
    struct vm_area_struct *vma
    );

/**
 * \brief Delegates processing of the control device's `unlocked_ioctl` and
 *        `compat_ioctl` file operations to the given device.
 *
 * ioctl commands known to the control device should not be given to this
 * function.
 *
 * THREAD-SAFETY: This function may be invoked concurrently with itself and with
 * any other function exposed by this component, with the exception that this
 * function may *not* be invoked concurrently on the same device with
 * `kbdus_device_destroy()`.
 *
 * SLEEPING: This function might sleep.
 */
int kbdus_device_handle_control_ioctl(
    struct kbdus_device *device,
    const struct kbdus_protocol *protocol,
    struct kbdus_protocol_instance *protocol_instance,
    unsigned int command,
    void __user *argument_usrptr
    );

/* -------------------------------------------------------------------------- */

#endif /* KBDUS_HEADER_DEVICE_H_ */
