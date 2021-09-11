#ifndef KBDUS_HEADER_PROTOCOL_H_
#define KBDUS_HEADER_PROTOCOL_H_

/* -------------------------------------------------------------------------- */
/* includes */

#include <kbdus.h>
#include <kbdus/inverter.h>

#include <linux/blkdev.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/uio.h>

/* -------------------------------------------------------------------------- */
/* component init / exit */

int kbdus_protocol_init(void) __init;

void kbdus_protocol_exit(void);

/* -------------------------------------------------------------------------- */
/* interface for protocol users */

/** \brief The implementation of a protocol. */
struct kbdus_protocol
{
    /**
     * \brief The name of the protocol.
     *
     * Must be at most 31 characters-long (excluding the null-terminator).
     */
    const char *name;

    /**
     * \brief Initializes the protocol.
     *
     * May be `NULL`.
     *
     * Called when module is loaded. As such, the implementation of this
     * callback may be marked `__init`.
     */
    int (*init)(void);

    /**
     * \brief Terminates the protocol.
     *
     * May be `NULL`.
     *
     * Called when module is unloaded. However, the implementation of this
     * callback may not be marked `__exit`, as it might also be called during
     * module initialization.
     */
    void (*exit)(void);

    /**
     * \brief Validate the protocol-specific configuration of a device.
     *
     * If this is `NULL`, then the protocol does not require any
     * protocol-specific configuration. Otherwise, the protocol requires
     * protocol-specific configuration and this callback is never invoked with
     * `NULL` as an argument.
     */
    // Validate the protocol-specific part of the given device configuration.
    bool (*validate_config)(const struct kbdus_config *config);

    /**
     * Called when the protocol instance is created. The protocol implementation
     * can use this to setup any necessary state.
     *
     * May be `NULL`.
     *
     * The returned value becomes the device's *protocol instance data*, which
     * is passed to callbacks that operate on the device.
     */
    void *(*create_instance)(const struct kbdus_config *config);

    /**
     * Called when the protocol instance is destroy. The protocol implementation
     * can use this to clean up any state of its own.
     *
     * May be `NULL`.
     */
    void (*destroy_instance)(void *instance_data);

    ssize_t (*handle_control_read_iter)(
        void *instance_data,
        struct kbdus_inverter *inverter,
        struct iov_iter *to
        );

    ssize_t (*handle_control_write_iter)(
        void *instance_data,
        struct kbdus_inverter *inverter,
        struct iov_iter *from
        );

    int (*handle_control_mmap)(
        void *instance_data,
        struct kbdus_inverter *inverter,
        struct vm_area_struct *vma
        );

    int (*handle_control_ioctl)(
        void *instance_data,
        struct kbdus_inverter *inverter,
        unsigned int command,
        void __user *argument_usrptr
        );
};

/**
 * \brief Returns the protocol with the given name, or `NULL` if no such
 *        protocol exists.
 */
const struct kbdus_protocol *kbdus_protocol_lookup(const char *protocol_name);

bool kbdus_protocol_validate_config(
    const struct kbdus_protocol *protocol,
    const struct kbdus_config *config
    );

/* -------------------------------------------------------------------------- */

struct kbdus_protocol_instance;

struct kbdus_protocol_instance *kbdus_protocol_create_instance(
    const struct kbdus_protocol *protocol,
    const struct kbdus_config *config
    );

void kbdus_protocol_destroy_instance(
    const struct kbdus_protocol *protocol,
    struct kbdus_protocol_instance *instance
    );

ssize_t kbdus_protocol_handle_control_read_iter(
    const struct kbdus_protocol *protocol,
    struct kbdus_protocol_instance *instance,
    struct kbdus_inverter *inverter,
    struct iov_iter *to
    );

ssize_t kbdus_protocol_handle_control_write_iter(
    const struct kbdus_protocol *protocol,
    struct kbdus_protocol_instance *instance,
    struct kbdus_inverter *inverter,
    struct iov_iter *from
    );

int kbdus_protocol_handle_control_mmap(
    const struct kbdus_protocol *protocol,
    struct kbdus_protocol_instance *instance,
    struct kbdus_inverter *inverter,
    struct vm_area_struct *vma
    );

int kbdus_protocol_handle_control_ioctl(
    const struct kbdus_protocol *protocol,
    struct kbdus_protocol_instance *instance,
    struct kbdus_inverter *inverter,
    unsigned int command,
    void __user *argument_usrptr
    );

/* -------------------------------------------------------------------------- */

#endif /* KBDUS_HEADER_PROTOCOL_H_ */
