#ifndef KBDUS_HEADER_CONFIG_H_
#define KBDUS_HEADER_CONFIG_H_

/* -------------------------------------------------------------------------- */
/* configuration */

/**
 * \brief (`int`) The hard maximum number of BDUS devices that can exist
 *        simultaneously.
 *
 * The `max_devices` module parameter cannot be greater than this value.
 */
#define KBDUS_HARD_MAX_DEVICES 4096

/**
 * \brief (`u32`) The hard maximum value for the `max_active_queue_reqs` device
 *        configuration attribute.
 */
#define KBDUS_HARD_MAX_ACTIVE_QUEUE_REQS 4096u

/**
 * \brief (`u32`) The hard maximum value for the `max_active_ioctl_reqs` device
 *        configuration attribute.
 */
#define KBDUS_HARD_MAX_ACTIVE_IOCTL_REQS 4096u

/**
 * \brief (`u32`) The maximum value for the `num_request_buffers` device
 *        configuration attribute.
 *
 * Used for the "mmap" protocol.
 */
#define KBDUSMMAP_MAX_REQUEST_BUFFERS 64u

/* -------------------------------------------------------------------------- */

#endif /* KBDUS_HEADER_CONFIG_H_ */
