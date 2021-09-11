#ifndef LIBBDUS_HEADER_BDUS_H_
#define LIBBDUS_HEADER_BDUS_H_

/* -------------------------------------------------------------------------- */
/* includes */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* -------------------------------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* driver development */

struct bdus_ops;
struct bdus_attrs;

/**
 * \brief Holds information about a device driver and its associated device.
 */
struct bdus_dev
{
    /** \brief The device's index. */
    const uint32_t index;

    /** \brief The device's path. */
    const char *const path;

    /** \brief The device's major number. */
    const uint32_t major;

    /** \brief The device's first minor number. */
    const uint32_t first_minor;

    /** \brief Driver callbacks. */
    const struct bdus_ops *const ops;

    /** \brief Device and driver attributes. */
    const struct bdus_attrs *const attrs;

    /** \brief Whether the driver is being run with `bdus_rerun()`. */
    const bool is_rerun;

    /**
     * \brief A pointer to be used freely by the device callbacks.
     *
     * The initial value for this field is given by the `user_data` parameter to
     * `bdus_run()` or `bdus_rerun()`.
     *
     * BDUS never inspects nor modifies the value of this field.
     */
    void *user_data;
};

/**
 * \brief Holds pointers to driver management and request processing callbacks.
 *
 * All callbacks are optional. Request types for which no callback is provided
 * are reported not to be supported by the device.
 *
 * See also <a href="developing-drivers.html#device-life-cycle">Device life
 * cycle</a>.
 */
struct bdus_ops
{
    /**
     * \brief Callback for initializing the driver.
     *
     * If set to `NULL`, this callback defaults to doing nothing.
     *
     * This callback is invoked on a driver before it starts serving any
     * requests (*i.e.*, before any other callback is invoked). It is invoked
     * only once.
     *
     * If this callback fails (by returning a non-zero value), `terminate()` is
     * *not* invoked and `bdus_run()` or `bdus_rerun()` returns.
     *
     * This callback is never run concurrently with any other callback on the
     * same device, and is guaranteed to be invoked before `bdus_run()`
     * daemonizes the current process.
     *
     * \param dev Information about the device and driver.
     *
     * \return On success, this callback should return 0. On failure, an `errno`
     *         value should be returned (*e.g.*, `ETIMEDOUT`).
     */
    int (*initialize)(struct bdus_dev *dev);

    /**
     * \brief Callback to be invoked after the device becomes available to
     * clients.
     *
     * If set to `NULL`, this callback defaults to outputting the device's path,
     * followed by a newline, to `stdout`.
     *
     * Note that device creation may fail, and as such this callback might never
     * be invoked. However, when this callback is invoked, it is guaranteed that
     * `initialize()` was previously run.
     *
     * Also note that it is possible for several request processing callbacks to
     * be run before the device becomes available to clients.
     *
     * If this callback fails (by returning a non-zero value), `terminate()` is
     * immediately invoked and `bdus_run()` or `bdus_rerun()` returns.
     *
     * This callback is never run concurrently with any other callback on the
     * same device, and is guaranteed to be invoked before the current process
     * is daemonized (which occurs if attribute `dont_daemonize` is `false`).
     *
     * When using `bdus_rerun()`, this callback is also invoked even if the
     * existing device was already available to clients.
     *
     * \param dev Information about the device and driver.
     *
     * \return On success, this callback should return 0. On failure, an `errno`
     *         value should be returned (*e.g.*, `ETIMEDOUT`).
     */
    int (*on_device_available)(struct bdus_dev *dev);

    /**
     * \brief Callback for terminating the driver.
     *
     * If set to `NULL`, this callback defaults to doing nothing.
     *
     * This callback is invoked on a driver after it has stopped serving
     * requests. After this callback is invoked, no other callbacks are run. It
     * is invoked only once.
     *
     * When this callback is invoked, it is guaranteed that `initialize()` was
     * previously run. However, if `initialize()` failed, this callback is not
     * invoked.
     *
     * Note that as the driver may be terminated before the device becomes
     * available to users, `on_device_available()` might not have run when this
     * callback is invoked.
     *
     * Note also that the driver may be killed or otherwise exit abnormally, and
     * as such this callback might never be invoked.
     *
     * This callback is never run concurrently with any other callback for the
     * same driver.
     *
     * \param dev Information about the device and driver.
     *
     * \return On success, this callback should return 0. On failure, an `errno`
     *         value should be returned (*e.g.*, `ETIMEDOUT`).
     */
    int (*terminate)(struct bdus_dev *dev);

    /**
     * \brief Callback for serving *read* requests.
     *
     * When BDUS invokes this callback, it is guaranteed that:
     *
     *   - \p buffer points to a buffer of \p size bytes, aligned in memory to
     *     the system's page size;
     *   - \p offset is a multiple of `dev->attrs->logical_block_size`;
     *   - \p size is a positive multiple of `dev->attrs->logical_block_size`;
     *   - `offset + size <= dev->attrs->size`;
     *   - `size <= dev->attrs->max_read_write_size`.
     *
     * \param buffer The buffer to which the data should be read.
     * \param offset The offset (in bytes) into the device at which the read
     *        should take place.
     * \param size The number of bytes that should be read.
     * \param dev Information about the device and driver.
     *
     * \return On success, this callback should return 0. On failure, an `errno`
     *         value should be returned (*e.g.*, `ETIMEDOUT`).
     */
    int (*read)(
        char *buffer, uint64_t offset, uint32_t size,
        struct bdus_dev *dev
        );

    /**
     * \brief Callback for serving *write* requests.
     *
     * When BDUS invokes this callback, it is guaranteed that:
     *
     *   - \p buffer points to a buffer of \p size bytes, aligned in memory to
     *     the system's page size;
     *   - \p offset is a multiple of `dev->attrs->logical_block_size`;
     *   - \p size is a positive multiple of `dev->attrs->logical_block_size`;
     *   - `offset + size <= dev->attrs->size`;
     *   - `size <= dev->attrs->max_read_write_size`.
     *
     * \param buffer A buffer containing the data to be written.
     * \param offset The offset (in bytes) into the device at which the write
     *        should take place.
     * \param size The number of bytes that should be written.
     * \param dev Information about the device and driver.
     *
     * \return On success, this callback should return 0. On failure, an `errno`
     *         value should be returned (*e.g.*, `ETIMEDOUT`).
     */
    int (*write)(
        const char *buffer, uint64_t offset, uint32_t size,
        struct bdus_dev *dev
        );

    /**
     * \brief Callback for serving *write same* requests.
     *
     * *Write same* requests are used to write the same data to several
     * contiguous logical blocks of the device.
     *
     * If this callback is not implemented but the `write` callback is, *write
     * same* requests will be transparently converted into equivalent *write*
     * requests.
     *
     * When BDUS invokes this callback, it is guaranteed that:
     *
     *   - \p buffer points to a buffer of `dev->attrs->logical_block_size`
     *     bytes, aligned in memory to the system's page size;
     *   - \p offset is a multiple of `dev->attrs->logical_block_size`;
     *   - \p size is a positive multiple of `dev->attrs->logical_block_size`;
     *   - `offset + size <= dev->attrs->size`;
     *   - `size <= dev->attrs->max_write_same_size`.
     *
     * \param buffer A buffer containing the data to be written to each logical
     *        block.
     * \param offset The offset (in bytes) into the device at which the write
     *        should take place.
     * \param size The number of bytes that should be written.
     * \param dev Information about the device and driver.
     *
     * \return On success, this callback should return 0. On failure, an `errno`
     *         value should be returned (*e.g.*, `ETIMEDOUT`).
     */
    int (*write_same)(
        const char *buffer, uint64_t offset, uint32_t size,
        struct bdus_dev *dev
        );

    /**
     * \brief Callback for serving *write zeros* requests.
     *
     * *Write zeros* requests are used to write zeros to a contiguous range of
     * the device.
     *
     * If `may_unmap` is `false`, the driver must ensure that subsequent writes
     * to the same range do not fail due to insufficient space. For example, for
     * a thin-provisioned device, space must be allocated for the zeroed range.
     *
     * If this callback is not implemented but the `write` callback is, *write
     * zeros* requests will be transparently converted into equivalent *write*
     * requests.
     *
     * When BDUS invokes this callback, it is guaranteed that:
     *
     *   - \p offset is a multiple of `dev->attrs->logical_block_size`;
     *   - \p size is a positive multiple of `dev->attrs->logical_block_size`;
     *   - `offset + size <= dev->attrs->size`;
     *   - `size <= dev->attrs->max_write_zeros_size`.
     *
     * \param offset The offset (in bytes) into the device at which the write
     *        should take place.
     * \param size The number of bytes that should be set to zero.
     * \param may_unmap Whether subsequent writes to the same range are allowed
     *        to fail due to insufficient space.
     * \param dev Information about the device and driver.
     *
     * \return On success, this callback should return 0. On failure, an `errno`
     *         value should be returned (*e.g.*, `ETIMEDOUT`).
     */
    int (*write_zeros)(
        uint64_t offset, uint32_t size, bool may_unmap,
        struct bdus_dev *dev
        );

    /**
     * \brief Callback for serving *FUA write* requests.
     *
     * *FUA write* requests are used to perform a Force Unit Access (FUA) write
     * to the device, meaning that the written data should reach persistent
     * storage before this callback returns.
     *
     * If this callback is implemented, then the `flush` callback must also be
     * implemented.
     *
     * When BDUS invokes this callback, it is guaranteed that:
     *
     *   - \p buffer points to a buffer of \p size bytes, aligned in memory to
     *     the system's page size;
     *   - \p offset is a multiple of `dev->attrs->logical_block_size`;
     *   - \p size is a positive multiple of `dev->attrs->logical_block_size`;
     *   - `offset + size <= dev->attrs->size`;
     *   - `size <= dev->attrs->max_read_write_size`.
     *
     * \param buffer A buffer containing the data to be written.
     * \param offset The offset (in bytes) into the device at which the write
     *        should take place.
     * \param size The number of bytes that should be written.
     * \param dev Information about the device and driver.
     *
     * \return On success, this callback should return 0. On failure, an `errno`
     *         value should be returned (*e.g.*, `ETIMEDOUT`).
     */
    int (*fua_write)(
        const char *buffer, uint64_t offset, uint32_t size,
        struct bdus_dev *dev
        );

    /**
     * \brief Callback for serving *flush* requests.
     *
     * *Flush* requests are used to flush the device's write-back cache.
     *
     * If this is callback is not implemented, it is assumed that the device
     * does not feature a write-back cache (implying that upon completion of
     * write requests, data is in permanent storage), and as such does not
     * require flushing.
     *
     * \param dev Information about the device and driver.
     *
     * \return On success, this callback should return 0. On failure, an `errno`
     *         value should be returned (*e.g.*, `ETIMEDOUT`).
     */
    int (*flush)(
        struct bdus_dev *dev
        );

    /**
     * \brief Callback for serving *discard* requests.
     *
     * When BDUS invokes this callback, it is guaranteed that:
     *
     *   - \p offset is a multiple of `dev->attrs->logical_block_size`;
     *   - \p size is a positive multiple of `dev->attrs->logical_block_size`;
     *   - `offset + size <= dev->attrs->size`;
     *   - `size <= dev->attrs->max_discard_erase_size`.
     *
     * \param offset The offset (in bytes) into the device at which the region
     *        to be discarded starts.
     * \param size The size (in bytes) of the region to be discarded.
     * \param dev Information about the device and driver.
     *
     * \return On success, this callback should return 0. On failure, an `errno`
     *         value should be returned (*e.g.*, `ETIMEDOUT`).
     */
    int (*discard)(
        uint64_t offset, uint32_t size,
        struct bdus_dev *dev
        );

    /**
     * \brief Callback for serving *secure erase* requests.
     *
     * When BDUS invokes this callback, it is guaranteed that:
     *
     *   - \p offset is a multiple of `dev->attrs->logical_block_size`;
     *   - \p size is a positive multiple of `dev->attrs->logical_block_size`;
     *   - `offset + size <= dev->attrs->size`;
     *   - `size <= dev->attrs->max_discard_erase_size`.
     *
     * \param offset The offset (in bytes) into the device at which the region
     *        to be erased starts.
     * \param size The size (in bytes) of the region to be erased.
     * \param dev Information about the device and driver.
     *
     * \return On success, this callback should return 0. On failure, an `errno`
     *         value should be returned (*e.g.*, `ETIMEDOUT`).
     */
    int (*secure_erase)(
        uint64_t offset, uint32_t size,
        struct bdus_dev *dev
        );

    /**
     * \brief Callback for serving *ioctl* requests.
     *
     * The value of `argument` depends on the *ioctl* command specified by the
     * `command` parameter. Assuming that `dir = _IOC_DIR(command)` and `size =
     * _IOC_SIZE(command)`:
     *
     * - If `dir == _IOC_NONE`, then \p argument is `NULL`;
     * - If `dir == _IOC_READ`, then \p argument points to a buffer of `size`
     *   bytes containing the argument data provided by the client who submitted
     *   the *ioctl* request, and changes to this data are ignored;
     * - If `dir == _IOC_WRITE`, then \p argument points to a zero-filled buffer
     *   of `size` bytes, and changes to this data are propagated to the
     *   argument of the client who submitted the *ioctl* request *if and only
     *   if* this callback returns 0;
     * - If `dir == _IOC_READ | _IOC_WRITE`, then \p argument points to a buffer
     *   of `size` bytes containing the argument data provided by the client who
     *   submitted the *ioctl* request, and changes to this data are propagated
     *   to the argument of the client *if and only if* this callback returns 0.
     *
     * \param command The *ioctl* command.
     * \param argument The argument.
     * \param dev Information about the device and driver.
     *
     * \return On success, this callback should return 0. On failure, an `errno`
     *         value should be returned (*e.g.*, `ETIMEDOUT`).
     */
    int (*ioctl)(
        uint32_t command, void *argument,
        struct bdus_dev *dev
        );
};

/** \brief Holds the attributes of a device and driver. */
struct bdus_attrs
{
    /**
     * \brief The device's logical block size, in bytes.
     *
     * This should be set to the smallest size that the driver is able to
     * address.
     *
     * When using `bdus_run()`, must be a power of two greater than or equal to
     * 512 and less than or equal to the system's page size.
     *
     * When using `bdus_rerun()`, must either be 0 or equal to the existing
     * device's logical block size. (If set to 0, this attribute's value in
     * `dev->attrs` as available from driver callbacks will be equal to the
     * existing device's logical block size.)
     */
    uint32_t logical_block_size;

    /**
     * \brief The device's physical block size, in bytes.
     *
     * This should be set to the smallest size that the driver can operate on
     * without reverting to read-modify-write operations.
     *
     * When using `bdus_run()`, must either be 0 or a power of two greater than
     * or equal to `logical_block_size` and less than or equal to the system's
     * page size. (If set to 0, this attribute's value in `dev->attrs` as
     * available from driver callbacks will be equal to `logical_block_size`.)
     *
     * When using `bdus_run()`, must either be 0 or equal to the existing
     * device's physical block size. (If set to 0, this attribute's value in
     * `dev->attrs` as available from driver callbacks will be equal to the
     * existing device's physical block size.)
     */
    uint32_t physical_block_size;

    /**
     * \brief The size of the device, in bytes.
     *
     * When using `bdus_run()`, must be a positive multiple of
     * `physical_block_size`, or of `logical_block_size` if the former is 0.
     *
     * When using `bdus_rerun()`, must either be 0 or equal to the existing
     * device's size. (If set to 0, this attribute's value in `dev->attrs` as
     * available from driver callbacks will be equal to the existing device's
     * size.)
     */
    uint64_t size;

    /**
     * \brief The maximum value for the `size` parameter of the `read`, `write`,
     *        and `fua_write` callbacks in `struct bdus_ops`.
     *
     * When using `bdus_run()`, must either be 0 or greater than or equal to the
     * system's page size.
     *
     * When using `bdus_rerun()`, must either be 0 or greater than or equal to
     * the original driver's value for this attribute.
     *
     * This attribute may take on a different value in `dev->attrs` as available
     * from driver callbacks, according to the following rules:
     *
     * - When using `bdus_run()`:
     *
     *   - If *none* of the `read`, `write`, and `fua_write` callbacks are
     *     implemented, this value is set to 0;
     *   - Otherwise, if this value is 0, it is set to an unspecified value
     *     greater than or equal to the system's page size;
     *   - Otherwise, this value is either left unmodified or decreased to an
     *     unspecified value that is greater than or equal to the system's page
     *     size (but never increased).
     *
     * - When using `bdus_rerun()`:
     *
     *   - If *none* of the `read`, `write`, and `fua_write` callbacks are
     *     implemented, this value is set to 0;
     *   - Otherwise, it is set to the original driver's value for this
     *     attribute.
     */
    uint32_t max_read_write_size;

    /**
     * \brief The maximum value for the `size` parameter of the `write_same`
     *        callback in `struct bdus_ops`.
     *
     * When using `bdus_run()`, must either be 0 or greater than or equal to
     * `logical_block_size`.
     *
     * When using `bdus_rerun()`, must either be 0 or greater than or equal to
     * the original driver's value for this attribute.
     *
     * This attribute may take on a different value in `dev->attrs` as available
     * from driver callbacks, according to the following rules:
     *
     * - When using `bdus_run()`:
     *
     *   - If the `write_same` callback is *not* implemented, this value is set
     *     to 0;
     *   - Otherwise, if this value is 0, it is set to an unspecified value
     *     greater than or equal to `logical_block_size`;
     *   - Otherwise, this value is either left unmodified or decreased to an
     *     unspecified value that is greater than or equal to
     *     `logical_block_size` (but never increased).
     *
     * - When using `bdus_rerun()`:
     *
     *   - If the `write_same` callback is *not* implemented, this value is set
     *     to 0;
     *   - Otherwise, it is set to the original driver's value for this
     *     attribute.
     */
    uint32_t max_write_same_size;

    /**
     * \brief The maximum value for the `size` parameter of the `write_zeros`
     *        callback in `struct bdus_ops`.
     *
     * When using `bdus_run()`, must either be 0 or greater than or equal to
     * `logical_block_size`.
     *
     * When using `bdus_rerun()`, must either be 0 or greater than or equal to
     * the original driver's value for this attribute.
     *
     * This attribute may take on a different value in `dev->attrs` as available
     * from driver callbacks, according to the following rules:
     *
     * - When using `bdus_run()`:
     *
     *   - If the `write_zeros` callback is *not* implemented, this value is set
     *     to 0;
     *   - Otherwise, if this value is 0, it is set to an unspecified value
     *     greater than or equal to `logical_block_size`;
     *   - Otherwise, this value is either left unmodified or decreased to an
     *     unspecified value that is greater than or equal to
     *     `logical_block_size` (but never increased).
     *
     * - When using `bdus_rerun()`:
     *
     *   - If the `write_zeros` callback is *not* implemented, this value is set
     *     to 0;
     *   - Otherwise, it is set to the original driver's value for this
     *     attribute.
     */
    uint32_t max_write_zeros_size;

    /**
     * \brief The maximum value for the `size` parameter of the `discard` and
     *        `secure_erase` callbacks in `struct bdus_ops`.
     *
     * When using `bdus_run()`, must either be 0 or greater than or equal to
     * `logical_block_size`.
     *
     * When using `bdus_rerun()`, must either be 0 or greater than or equal to
     * the original driver's value for this attribute.
     *
     * This attribute may take on a different value in `dev->attrs` as available
     * from driver callbacks, according to the following rules:
     *
     * - When using `bdus_run()`:
     *
     *   - If both the `discard` and `secure_erase` callbacks are *not*
     *     implemented, this value is set to 0;
     *   - Otherwise, if this value is 0, it is set to an unspecified value
     *     greater than or equal to `logical_block_size`;
     *   - Otherwise, this value is either left unmodified or decreased to an
     *     unspecified value that is greater than or equal to
     *     `logical_block_size` (but never increased).
     *
     * - When using `bdus_rerun()`:
     *
     *   - If both the `discard` and `secure_erase` callbacks are *not*
     *     implemented, this value is set to 0;
     *   - Otherwise, it is set to the original driver's value for this
     *     attribute.
     */
    uint32_t max_discard_erase_size;

    /**
     * \brief The maximum number of invocations of the callbacks in `struct
     *        bdus_ops` that may be in progress simultaneously.
     *
     * This attribute may take on a different value in `dev->attrs` as available
     * from driver callbacks, according to the following rules:
     *
     * - If this value is 0, it is set to 1.
     * - Otherwise, this value is either left unmodified or decreased to an
     *   unspecified positive value (but never increased).
     *
     * If this attribute's value (as available in `dev->attrs` from driver
     * callbacks) is 1, then callbacks are never invoked concurrently, and it is
     * also guaranteed that callbacks are always invoked from the thread that is
     * running `bdus_run()` or `bdus_rerun()` (disregarding daemonization).
     */
    uint32_t max_concurrent_callbacks;

    /**
     * \brief Whether to disable partition scanning for the device.
     *
     * If `true`, the kernel will never attempt to recognize partitions in the
     * device. This is useful to ensure that data in unpartitioned devices is
     * not mistakenly interpreted as partitioning information.
     *
     * When using `bdus_rerun()`, this attribute is ignored and its value in
     * `dev->attrs` as available from driver callbacks will be the value given
     * by the device's original driver.
     */
    bool disable_partition_scanning;

    /**
     * \brief Whether *not* to destroy the device in case of driver failure.
     *
     * If this is `false` and the driver terminates abnormally, the
     * corresponding device is automatically destroyed. On the other hand, if
     * this is `true`, the device continues to exist without a controlling
     * driver, allowing another driver to take control of the device by using
     * `bdus_rerun()`.
     *
     * Note that regardless of the value of this attribute, `bdus_rerun()` can
     * be used to replace the driver for a device that already has a controlling
     * driver.
     *
     * When using `bdus_rerun()`, this attribute is ignored and its value in
     * `dev->attrs` as available from driver callbacks will be the value given
     * by the device's original driver.
     */
    bool recoverable;

    /**
     * \brief Whether *not* to daemonize the process calling `bdus_run()` after
     *        the device becomes available.
     *
     * If this is `false`, then the process that invokes `bdus_run()` or
     * `bdus_rerun()` will be daemonized after the `on_device_available()`
     * callback (or its default implementation) is invoked and completes.
     *
     * After daemonization, `stdin`, `stdout`, and `stderr` are redirected to
     * `/dev/null`. Note, however, that the current working directory and umask
     * are not changed.
     */
    bool dont_daemonize;

    /**
     * \brief Whether to log callback invocations.
     *
     * If `true`, a message will be printed to `stderr` immediately before every
     * callback invocation. Note that messages printed after the driver is
     * daemonized are not visible, since `stderr` is redirected to `/dev/null`.
     */
    bool log_callbacks;
};

/**
 * \brief Runs a driver for a new block device with the specified callbacks and
 *        attributes.
 *
 * This function fails if the effective user ID of the calling process does not
 * correspond to the `root` user. However, the effective user ID may be modified
 * before this function returns (*e.g.*, in the `ops->initialize()` callback).
 *
 * Concurrent invocations of this function with itself and with `bdus_rerun()`
 * are allowed only if `attrs->dont_daemonize` is `true` for all of them.
 *
 * \param ops Driver callbacks.
 * \param attrs Device and driver attributes.
 * \param user_data The initial value for the `user_data` field of the `struct
 *        bdus_dev` given to the driver's callbacks.
 *
 * \return On success, blocks until the driver is terminated and returns `true`.
 *         On failure, `false` is returned, `errno` is set to an appropriate
 *         error number, and the current error message is set to a string
 *         descriptive of the error (see `bdus_get_error_message()`).
 */
bool bdus_run(
    const struct bdus_ops *ops,
    const struct bdus_attrs *attrs,
    void *user_data
    );

/**
 * \brief Runs a driver for an *existing* block device with the specified
 *        callbacks and attributes.
 *
 * The existing device may or may not already have a controlling driver. If it
 * does, the controlling driver is sent a flush request and then terminated
 * before the new driver is initialized.
 *
 * If the existing driver terminates abnormally (*e.g.*, crashes or its
 * `terminate()` callback returns an error) and its `recoverable` attribute is
 * `false`, this function fails and the device is subsequently destroyed.
 * However, if its `recoverable` attribute is `true`, the new driver is
 * initialized regardless of whether the existing driver terminates
 * successfully.
 *
 * The new driver must support the same request types as the existing device's
 * original driver. Callbacks for serving other request types may also be
 * provided, but are not used.
 *
 * This function fails if the effective user ID of the calling process does not
 * correspond to the `root` user. However, the effective user ID may be modified
 * before this function returns (*e.g.*, in the `ops->initialize()` callback).
 *
 * Concurrent invocations of this function with itself and with `bdus_run()` are
 * allowed only if `attrs->dont_daemonize` is `true` for all of them.
 *
 * \param dev_index The index of the device.
 * \param ops Driver callbacks.
 * \param attrs Device and driver attributes.
 * \param user_data The initial value for the `user_data` field of the `struct
 *        bdus_dev` given to the driver's callbacks.
 *
 * \return On success, blocks until the driver is terminated and returns `true`.
 *         On failure, `false` is returned, `errno` is set to an appropriate
 *         error number, and the current error message is set to a string
 *         descriptive of the error (see `bdus_get_error_message()`).
 */
bool bdus_rerun(
    uint32_t dev_index,
    const struct bdus_ops *ops,
    const struct bdus_attrs *attrs,
    void *user_data
    );

/**
 * \brief Specifies internal configuration parameters.
 *
 * Note that this type is *not* part of the library's public API, and as such no
 * backward compatibility guarantees are provided.
 */
struct bdus_internal_config_
{
    /**
     * \brief The name of the backend to be used.
     *
     * If `NULL`, defaults to `"mmap"`.
     */
    const char *backend_name;

    /**
     * \brief Whether the device is *not* reported to the Linux kernel as being
     *        "rotational".
     *
     * This may have an influence on request sorting, merging, and scheduling.
     *
     * When attaching to an existing device, the previous value is kept and this
     * value is ignored.
     *
     * Applies to all backends.
     */
    bool is_not_rotational;

    /**
     * \brief Whether request merging should *not* be performed by the Linux
     * kernel.
     *
     * When attaching to an existing device, the previous value is kept and this
     * value is ignored.
     *
     * Applies to all backends.
     */
    bool dont_merge_requests;

    /**
     * \brief TODO: document
     *
     * - If `true`, thread that asks for request first gets next request.
     * - If `false`, thread that asks for request last gets next request.
     *
     * When attaching to an existing device, the previous value is kept and this
     * value is ignored.
     *
     * Applies to all backends.
     */
    bool fifo_request_transmission;

    /**
     * \brief The maximum amount of *active* queue requests.
     *
     * This effectively corresponds to the maximum allowed number of "queue"
     * requests that *libbdus* has obtained from the kernel but not yet replied
     * to.
     *
     * When attaching to an existing device, the previous value is kept and this
     * value is ignored.
     *
     * Applies to all backends.
     *
     * If `0`, defaults to `64`.
     */
    uint32_t max_active_queue_reqs;

    /**
     * \brief The maximum amount of *active* ioctl requests.
     *
     * This effectively corresponds to the maximum allowed number of "ioctl"
     * requests that *libbdus* has obtained from the kernel but not yet replied
     * to.
     *
     * When attaching to an existing device, the previous value is kept and this
     * value is ignored.
     *
     * Applies to all backends.
     *
     * If `0`, defaults to `16`.
     */
    uint32_t max_active_ioctl_reqs;
};

/**
 * \brief Same as `bdus_run()` but accepts a custom internal configuration
 *        instead of using the default one.
 *
 * Note that this function is *not* part of the library's public API, and as
 * such no backward compatibility guarantees are provided.
 *
 * \param ops Same as in `bdus_run()`.
 * \param attrs Same as in `bdus_run()`.
 * \param user_data Same as in `bdus_run()`.
 * \param internal_config The custom internal configuration.
 */
bool bdus_run_with_internal_config_(
    const struct bdus_ops *ops,
    const struct bdus_attrs *attrs,
    void *user_data,
    const struct bdus_internal_config_ *internal_config
    );

/**
 * \brief Same as `bdus_rerun()` but accepts a custom internal configuration
 *        instead of using the default one.
 *
 * Note that this function is *not* part of the library's public API, and as
 * such no backward compatibility guarantees are provided.
 *
 * \param dev_index Same as in `bdus_rerun()`.
 * \param ops Same as in `bdus_rerun()`.
 * \param attrs Same as in `bdus_rerun()`.
 * \param user_data Same as in `bdus_rerun()`.
 * \param internal_config The custom internal configuration.
 */
bool bdus_rerun_with_internal_config_(
    uint32_t dev_index,
    const struct bdus_ops *ops,
    const struct bdus_attrs *attrs,
    void *user_data,
    const struct bdus_internal_config_ *internal_config
    );

/* -------------------------------------------------------------------------- */
/* device management */

/**
 * \brief Flushes a device.
 *
 * This function blocks until all data previously written to the device is
 * persistently stored (or until an error occurs). This has the same effect as
 * performing an `fsync()` or `fdatasync()` call on the device, or executing the
 * `sync` command with a path to the block device as an argument.
 *
 * This function fails if the effective user ID of the calling process does not
 * correspond to the `root` user.
 *
 * \param dev_index The index of the device to be flushed.
 *
 * \return On success, returns `true`. On failure, `false` is returned, `errno`
 *         is set to an appropriate error number, and the current error message
 *         is set to a string descriptive of the error (see
 *         `bdus_get_error_message()`).
 */
bool bdus_flush_dev(uint32_t dev_index);

/**
 * \brief Destroys a device.
 *
 * This function first prompts the device's driver to terminate (if it has a
 * controlling driver), waits until it does, and then destroys the device. (or
 * until an error occurs). Note that the driver might be terminated and the
 * device destroyed even if this function fails.
 *
 * This function fails if the effective user ID of the calling process does not
 * correspond to the `root` user.
 *
 * Calling this function with \p flush_dev set to `true` is similar to first
 * invoking `bdus_flush_dev()` and then this function with \p flush_dev set to
 * `false`, but avoids the race condition in which the device may be destroyed
 * and its index reused for another device in between the two calls.
 *
 * \param dev_index The index of the device to be destroyed.
 * \param flush_dev Whether to flush the device before terminating its driver.
 *
 * \return On success, returns `true`. On failure, `false` is returned, `errno`
 *         is set to an appropriate error number, and the current error message
 *         is set to a string descriptive of the error (see
 *         `bdus_get_error_message()`).
 */
bool bdus_destroy_dev(uint32_t dev_index, bool flush_dev);

/* -------------------------------------------------------------------------- */
/* device indices and paths */

/**
 * \brief Gets the path to the device with the specified index.
 *
 * This is a purely syntactical operation. In particular, the existence of the
 * device in question is not checked.
 *
 * If \p path_buf_size is positive, the device's path is written to \p path_buf,
 * and this function fails if the computed path (including the null terminator)
 * does not fit in that buffer.
 *
 * If \p path_buf_size is 0, nothing is written to the buffer and \p path_buf
 * may be `NULL`. However, the length of the device's path (excluding the null
 * terminator) is still returned.
 *
 * \param path_buf The buffer to which the device's path should be written.
 * \param path_buf_size The maximum number of characters to be written to
 *        \p path_buf (including the null terminator).
 * \param dev_index The device's index.
 *
 * \return On success, the number of characters (excluding the null terminator)
 *         that were (or would be) written to \p path_buf is returned. On
 *         failure, 0 is returned, `errno` is set to an appropriate error
 *         number, and the current error message is set to a string descriptive
 *         of the error (see `bdus_get_error_message()`).
 */
size_t bdus_dev_index_to_path(
    char *path_buf,
    size_t path_buf_size,
    uint32_t dev_index
    );

/**
 * \brief Gets the index of the device with the specified path.
 *
 * This is a purely syntactical operation. In particular, the existence of the
 * device in question is not checked and the given path must follow the format
 * `/dev/bdus-N`, where `N` is the device index.
 *
 * If this function fails, the value of `*out_dev_index` is not changed.
 *
 * \param out_dev_index Pointer to a variable in which the device's index should
 *        be stored.
 * \param dev_path The device's path.
 *
 * \return On success, the device's index is written to \p out_dev_index and
 *         `true` is returned. If \p dev_path is not a valid device path,
 *         `false` is returned, `errno` is set to `EINVAL`, and the current
 *         error message is set to a string descriptive of the error (see
 *         `bdus_get_error_message()`).
 */
bool bdus_dev_path_to_index(
    uint32_t *out_dev_index,
    const char *dev_path
    );

/**
 * \brief Gets the index of the device with the specified index or path.
 *
 * This is a purely syntactical operation. In particular, the existence of the
 * device in question is not checked and, if the given string is a device path,
 * it must follow the format `/dev/bdus-N`, where `N` is the device index.
 *
 * Unlike `bdus_dev_path_to_index()`, this function also succeeds if the
 * specified string can be parsed as a non-negative integer.
 *
 * If this function fails, the value of `*out_dev_index` is not changed.
 *
 * \param out_dev_index Pointer to a variable in which the device's index should
 *        be stored.
 * \param dev_index_or_path The device's index or path.
 *
 * \return On success, the device's index is written to \p out_dev_index and
 *         `true` is returned. If \p dev_path is not a valid device index or
 *         path, `false` is returned, `errno` is set to `EINVAL`, and the
 *         current error message is set to a string descriptive of the error
 *         (see `bdus_get_error_message()`).
 */
bool bdus_dev_index_or_path_to_index(
    uint32_t *out_dev_index,
    const char *dev_index_or_path
    );

/* -------------------------------------------------------------------------- */
/* errors */

/**
 * \brief Returns the current error message of the calling thread.
 *
 * Every function exported by the BDUS user-space library (*libbdus*) sets this
 * string to a descriptive message when it fails. The current error message is
 * not changed under any other circumstance.
 *
 * Each thread has its own current error message, which is an empty string if no
 * error has occurred in the thread.
 *
 * Note that messages returned by this function may differ between releases,
 * even if they have the same *major* version.
 *
 * \return A string descriptive of the last error that occurred in the current
 *         thread.
 */
const char *bdus_get_error_message(void);

/* -------------------------------------------------------------------------- */
/* version */

/** \brief Represents a version number. */
struct bdus_version
{
    /** \brief The *major* version. */
    uint32_t major;

    /** \brief The *minor* version. */
    uint32_t minor;

    /** \brief The *patch* version. */
    uint32_t patch;
};

/**
 * \brief Returns the version of *libbdus* against which the calling application
 *        is linked.
 *
 * \return The version of *libbdus* against which the calling application is
 *         linked.
 */
const struct bdus_version *bdus_get_libbdus_version(void);

/**
 * \brief Gets the version of *kbdus* that is currently installed.
 *
 * This function fails if the effective user ID of the calling process does not
 * correspond to the `root` user.
 *
 * \param out_kbdus_version Pointer to a variable in which kbdus' version should
 *        be stored.
 *
 * \return On success, the version of *kbdus* that is currently installed is
 *         written to \p out_kbdus_version and `true` is returned. On failure,
 *         `false` is returned, `errno` is set to an appropriate error number,
 *         and the current error message is set to a string descriptive of the
 *         error (see `bdus_get_error_message()`).
 */
bool bdus_get_kbdus_version(struct bdus_version *out_kbdus_version);

/* -------------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

/* -------------------------------------------------------------------------- */

#endif /* LIBBDUS_HEADER_BDUS_H_ */
