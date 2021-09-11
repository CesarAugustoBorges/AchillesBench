#ifndef KBDUS_HEADER_INVERTER_H_
#define KBDUS_HEADER_INVERTER_H_

/* -------------------------------------------------------------------------- */
/* includes */

#include <kbdus.h>

#include <linux/blkdev.h>
#include <linux/init.h>
#include <linux/types.h>

/* -------------------------------------------------------------------------- */
/* component init / exit */

/** TODO: document */

/**
 * kbdus_inverter_init() - TODO.
 *
 * Return: TODO.
 */
int kbdus_inverter_init(void) __init;

/**
 * kbdus_inverter_exit() - TODO.
 */
void kbdus_inverter_exit(void);

/* -------------------------------------------------------------------------- */
/* common interface */

/**
 * struct kbdus_inverter - TODO.
 */
struct kbdus_inverter;

/* -------------------------------------------------------------------------- */
/* interface for request providers */

/**
 * kbdus_inverter_create() - TODO.
 * @max_active_reqs: TODO.
 * @device_config: TODO.
 *
 * CONTEXT: Must be called from process context.
 *
 * Return: TODO.
 */
struct kbdus_inverter *kbdus_inverter_create(
    u32 max_active_reqs,
    const struct kbdus_device_config *device_config
    );

/**
 * kbdus_inverter_destroy() - TODO.
 * @inverter: The inverter.
 *
 * CONTEXT: Must be called from process context.
 */
void kbdus_inverter_destroy(struct kbdus_inverter *inverter);

/**
 * kbdus_inverter_terminate() - TODO.
 * @inverter: The inverter.
 *
 * Must be called at least once before destroying instance. Can be called one or
 * more times.
 *
 * Cause all pending requests to fail with -EIO, and any new pushed requests
 * will also fail immediately with -EIO. Also makes available infinite
 * termination requests.
 *
 * Also causes pending and future ioctls to fail with -EIO.
 *
 * CONCURRENCY: This function may be invoked concurrently with itself and with
 * any other function exposed by this component, with the exception that this
 * function may *not* be invoked concurrently on the same inverter with
 * `kbdus_inverter_destroy()`.
 *
 * CONTEXT: Must be called from process context.
 *
 * SLEEPING: Never sleeps.
 */
void kbdus_inverter_terminate(struct kbdus_inverter *inverter);

/**
 * kbdus_inverter_deactivate() - TODO.
 * @inverter: The inverter.
 * @flush: Whether a "flush" request should be sent before the infinite
 *     termination requests.
 *
 * When deactivated, request pullers get infinite termination requests. They can
 * still complete previously gotten requests, and request pushers can still
 * submit requests, but these are kept on hold.
 *
 * When reactivated, pushed requests become again available for pullers.
 *
 * Does nothing if the inverter is already inactive.
 *
 * Must *not* be called if inverter was already terminated.
 *
 * CONCURRENCY: This function may be invoked concurrently with itself and with
 * any other function exposed by this component, with the exception that this
 * function may *not* be invoked concurrently on the same inverter with
 * `kbdus_inverter_destroy()`.
 *
 * CONTEXT: Must be called from process context.
 *
 * SLEEPING: Never sleeps.
 */
void kbdus_inverter_deactivate(struct kbdus_inverter *inverter, bool flush);

/**
 * kbdus_inverter_activate() - TODO.
 * @inverter: The inverter.
 *
 * If was previously inactive, then any requests that were "awaiting completion"
 * become "awaiting get".
 *
 * Does nothing if the inverter is already active.
 *
 * Must *not* be called if inverter was already terminated.
 *
 * CONCURRENCY: This function may be invoked concurrently with itself and with
 * any other function exposed by this component, with the exception that this
 * function may *not* be invoked concurrently on the same inverter with
 * `kbdus_inverter_destroy()`.
 *
 * CONTEXT: Must be called from process context.
 *
 * SLEEPING: Never sleeps.
 */
void kbdus_inverter_activate(struct kbdus_inverter *inverter);

/**
 * kbdus_inverter_submit_device_available_request() - TODO.
 * @inverter: The inverter.
 *
 * May be called 0 or more times. Even after terminating inverter. Has no effect
 * if called after terminating inverter. Never sleeps. Has no effect if a
 * "device available" request is already available.
 *
 * CONCURRENCY: This function may be invoked concurrently with itself and with
 * any other function exposed by this component, with the exception that this
 * function may *not* be invoked concurrently on the same inverter with
 * `kbdus_inverter_destroy()`.
 *
 * CONTEXT: Must be called from process context.
 *
 * SLEEPING: Never sleeps.
 */
void kbdus_inverter_submit_device_available_request(
    struct kbdus_inverter *inverter
    );

/**
 * kbdus_inverter_submit_queue_request() - TODO.
 * @inverter: The inverter.
 * @queue_req: TODO.
 * @out_request_handle_index: TODO.
 * @out_request_handle_seqnum: TODO.
 *
 * The request is later completed by putting a negated errno value in an `int`
 * in the first 4 bytes of the queue request's PDU.
 *
 * Takes a request in the "free" state, initializes it to represent the given
 * queue request, and puts it in "awaiting get" state. Returns the request
 * handle.
 *
 * Returns 0 (the null handle, meaning no request) if the request is failed by
 * this function because the inverter has been terminated.
 *
 * CONCURRENCY: This function may be invoked concurrently with itself and with
 * any other function exposed by this component, with the exception that this
 * function may *not* be invoked concurrently on the same inverter with
 * `kbdus_inverter_destroy()`.
 *
 * This also calls blk_mq_start_request() on the request.
 *
 * CONTEXT: Don't care.
 *
 * SLEEPING: Never sleeps.
 *
 * Return: TODO.
 */
int kbdus_inverter_submit_queue_request(
    struct kbdus_inverter *inverter,
    struct request *queue_req,
    u16 *out_request_handle_index,
    u64 *out_request_handle_seqnum
    );

/**
 * kbdus_inverter_timeout_queue_request() - Fails the given request due to time
 *     out.
 * @inverter: The inverter.
 * @request_handle_index: TODO.
 * @request_handle_seqnum: TODO.
 *
 * A negated errno value is put in an `int` in the first 4 bytes of the queue
 * request's PDU.
 *
 * CONCURRENCY: This function may be invoked concurrently with itself and with
 * any other function exposed by this component, with the exception that this
 * function may *not* be invoked concurrently on the same inverter with
 * `kbdus_inverter_destroy()`.
 *
 * CONTEXT: Don't care.
 *
 * SLEEPING: Never sleeps.
 *
 * Return: A value suitable to be returned by the `timeout` callback of `struct
 *     blk_mq_ops`.
 */
enum blk_eh_timer_return kbdus_inverter_timeout_queue_request(
    struct kbdus_inverter *inverter,
    u16 request_handle_index,
    u64 request_handle_seqnum
    );

/**
 * kbdus_inverter_submit_and_await_ioctl_request() - TODO.
 * @inverter: The inverter.
 * @command: TODO.
 * @argument_usrptr: TODO.
 *
 * CONCURRENCY: This function may be invoked concurrently with itself and with
 * any other function exposed by this component, with the exception that this
 * function may *not* be invoked concurrently on the same inverter with
 * `kbdus_inverter_destroy()`.
 *
 * CONTEXT: Must be called from process context.
 *
 * SLEEPING: Might sleep.
 *
 * Return: An error that should be used as the result of the ioctl call (which
 *     can also be also be -ERESTARTSYS).
 */
int kbdus_inverter_submit_and_await_ioctl_request(
    struct kbdus_inverter *inverter,
    unsigned int command,
    void __user *argument_usrptr
    );

/* -------------------------------------------------------------------------- */
/* interface for request processors */

/**
 * struct kbdus_inverter_request - TODO.
 */
struct kbdus_inverter_request
{
    /** @handle_seqnum: TODO. */
    u64 handle_seqnum;

    /** @handle_index: TODO. */
    u16 handle_index;

    /** @type: TODO. */
    u16 type;

    /** @ioctl_req_command: TODO. */
    unsigned int ioctl_req_command;

    union
    {
        /** @queue_req: TODO. */
        struct request *queue_req;

        /** @ioctl_req_argument_buffer: TODO. */
        void *ioctl_req_argument_buffer;
    };

    // pad to 24 bytes
#if BITS_PER_LONG == 32
    u8 padding_[4];
#endif
};

/**
 * kbdus_inverter_begin_request_get() - TODO.
 * @inverter: The inverter.
 *
 * Blocks until a request is available to be processed, and returns a pointer to
 * that request.
 *
 * Takes a request in the "awaiting get" state and puts it in the "being gotten"
 * state.
 *
 * CONCURRENCY: This function may be invoked concurrently with itself and with
 * any other function exposed by this component, with the exception that this
 * function may *not* be invoked concurrently on the same inverter with
 * `kbdus_inverter_destroy()`.
 *
 * CONTEXT: Must be called from process context.
 *
 * SLEEPING: Might sleep.
 *
 * Return: TODO.
 *
 * Returns ERR_PTR(-ERESTARTSYS) if interrupted.
 */
const struct kbdus_inverter_request *kbdus_inverter_begin_request_get(
    struct kbdus_inverter *inverter
    );

/**
 * kbdus_inverter_commit_request_get() - TODO.
 * @inverter: The inverter.
 * @request: TODO.
 *
 * CONCURRENCY: TODO.
 *
 * CONTEXT: Must be called from process context.
 *
 * SLEEPING: Never sleeps.
 *
 * Return: TODO.
 */
void kbdus_inverter_commit_request_get(
    struct kbdus_inverter *inverter,
    const struct kbdus_inverter_request *request
    );

/**
 * kbdus_inverter_abort_request_get() - TODO.
 * @inverter: The inverter.
 * @request: TODO.
 *
 * Takes a request in the "being gotten" state and puts it in the "awaiting get"
 * state.
 *
 * CONCURRENCY: TODO.
 *
 * CONTEXT: Must be called from process context.
 *
 * SLEEPING: Never sleeps.
 *
 * Return: TODO.
 */
void kbdus_inverter_abort_request_get(
    struct kbdus_inverter *inverter,
    const struct kbdus_inverter_request *request
    );

/**
 * kbdus_inverter_begin_request_completion() - TODO.
 * @inverter: The inverter.
 * @request_handle_index: TODO.
 * @request_handle_seqnum: TODO.
 *
 * Takes a request in the "awaiting completion" state and puts it in the "being
 * completed" state.
 *
 * CONCURRENCY: TODO.
 *
 * CONTEXT: Must be called from process context.
 *
 * SLEEPING: Never sleeps.
 *
 * Return: TODO.
 *
 * Returns NULL if the request was already completed (can happen due to timeouts
 * or cancellations).
 *
 * Returns ERR_PTR(-EINVAL) if the request_handle is invalid.
 */
const struct kbdus_inverter_request *kbdus_inverter_begin_request_completion(
    struct kbdus_inverter *inverter,
    u16 request_handle_index,
    u64 request_handle_seqnum
    );

/**
 * kbdus_inverter_commit_request_completion() - TODO.
 * @inverter: The inverter.
 * @request: TODO.
 * @negated_errno: TODO.
 *
 * CONCURRENCY: TODO.
 *
 * CONTEXT: Must be called from process context.
 *
 * SLEEPING: Never sleeps.
 *
 * Return: TODO.
 */
void kbdus_inverter_commit_request_completion(
    struct kbdus_inverter *inverter,
    const struct kbdus_inverter_request *request,
    int negated_errno
    );

/**
 * kbdus_inverter_abort_request_completion() - TODO.
 * @inverter: The inverter.
 * @request: TODO.
 *
 * Takes a request in the "being completed" state and puts it in the "awaiting
 * completion" state.
 *
 * CONCURRENCY: TODO.
 *
 * CONTEXT: Must be called from process context.
 *
 * SLEEPING: Never sleeps.
 *
 * Return: TODO.
 */
void kbdus_inverter_abort_request_completion(
    struct kbdus_inverter *inverter,
    const struct kbdus_inverter_request *request
    );

/* -------------------------------------------------------------------------- */

#endif /* KBDUS_HEADER_INVERTER_H_ */
