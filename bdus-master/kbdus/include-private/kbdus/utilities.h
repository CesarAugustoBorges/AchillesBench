#ifndef KBDUS_HEADER_UTILITIES_H_
#define KBDUS_HEADER_UTILITIES_H_

/* -------------------------------------------------------------------------- */
/* includes */

#include <linux/compiler.h>
#include <linux/completion.h>
#include <linux/list.h>
#include <linux/printk.h>
#include <linux/stringify.h>
#include <linux/uaccess.h>
#include <linux/version.h>

/* -------------------------------------------------------------------------- */
/* math */

// x must be of unsigned type; x may be evaluated multiple times
/** TODO: document */
#define kbdus_is_zero_or_power_of_two(x) (((x) & ((x) - 1)) == 0)

// x must be of unsigned type; x may be evaluated multiple times
/** TODO: document */
#define kbdus_is_power_of_two(x) (((x) & ((x) - 1)) == 0 && (x) != 0)

// x and y must be of unsigned type
/** TODO: document */
#define kbdus_is_zero_or_multiple_of(x, y) (((x) % (y)) == 0)

// x and y must be of unsigned type; x may be evaluated multiple times
/** TODO: document */
#define kbdus_is_positive_multiple_of(x, y) (((x) % (y)) == 0 && (x) != 0)

/* -------------------------------------------------------------------------- */
/* miscellaneous */

// This has linear complexity be costly and is only intended to be used in
// assertions.
/** TODO: document */
int kbdus_list_length(const struct list_head *head);

/* -------------------------------------------------------------------------- */
/* hacks */

/** TODO: document */
int kbdus_wait_for_completion_interruptible_lifo(struct completion *x);

/* -------------------------------------------------------------------------- */
/* compatibility utilities */

/** TODO: document */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0)
    #define KBDUS_VERIFY_READ
    #define KBDUS_VERIFY_WRITE
    #define kbdus_access_ok(type, addr, size) access_ok((addr), (size))
#else
    #define KBDUS_VERIFY_READ VERIFY_READ
    #define KBDUS_VERIFY_WRITE VERIFY_WRITE
    #define kbdus_access_ok(type, addr, size) access_ok((type), (addr), (size))
#endif

/* -------------------------------------------------------------------------- */
/* assertions and logging */

// condition is always evaluated exactly once
/** TODO: document */
#define kbdus_assert(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            printk( \
                KERN_ALERT "kbdus: " __FILE__ ":" __stringify(__LINE__) ":" \
                " assertion failed, system may be in an inconsistent state: " \
                __stringify(condition) "\n" \
                ); \
        } \
    } \
    while (0)

// kbdus_assert_if_debug evaluates condition 0 or 1 times
/** TODO: document */
#if KBDUS_DEBUG
    #define kbdus_assert_if_debug(condition) kbdus_assert(condition)
#else
    #define kbdus_assert_if_debug(condition) do { } while (0)
#endif

// Logs a generic message. (KERN_INFO)
/** TODO: document */
#if KBDUS_DEBUG
    #define kbdus_log_if_debug(message_format, ...) \
        do \
        { \
            printk( \
                KERN_DEBUG "kbdus: " __FILE__ ":" __stringify(__LINE__) ": " \
                message_format "\n", ## __VA_ARGS__ \
                ); \
        } \
        while (0)
#else
    #define kbdus_log_if_debug(message_format, ...) do { } while (0)
#endif

/* -------------------------------------------------------------------------- */

#endif /* KBDUS_HEADER_UTILITIES_H_ */
