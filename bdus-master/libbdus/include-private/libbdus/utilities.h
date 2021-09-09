#ifndef LIBBDUS_HEADER_UTILITIES_H_
#define LIBBDUS_HEADER_UTILITIES_H_

/* -------------------------------------------------------------------------- */
/* includes */

#include <stdbool.h>
#include <stdio.h>

/* -------------------------------------------------------------------------- */
/* symbol visibility */

#define BDUS_EXPORT_ __attribute__(( visibility("default") ))

/* -------------------------------------------------------------------------- */
/* logging */

#define bdus_log_(format, ...) \
    do { \
        fprintf(stderr, "log: " format "\n", __VA_ARGS__); \
        fflush(stderr); \
    } while (0)

#define bdus_log_no_args_(format) \
    do { \
        fprintf(stderr, "log: " format "\n"); \
        fflush(stderr); \
    } while (0)

#define bdus_log_thread_(thread, format, ...) \
    do { bdus_log_("thread %02d: " format, thread, __VA_ARGS__); } while (0)

#define bdus_log_thread_no_args_(thread, format) \
    do { bdus_log_("thread %02d: " format, thread); } while (0)

/* -------------------------------------------------------------------------- */
/* math */

// x and y may be evaluated multiple times
#define bdus_min_(x, y) ((x) < (y) ? (x) : (y))

// x and y may be evaluated multiple times
#define bdus_max_(x, y) ((x) > (y) ? (x) : (y))

// x must be of unsigned type; x may be evaluated multiple times
#define bdus_is_power_of_two_(x) ((x) != 0 && ((x) & ((x) - 1)) == 0)

// x must be of unsigned type; x may be evaluated multiple times
#define bdus_is_zero_or_power_of_two_(x) ((x) == 0 || ((x) & ((x) - 1)) == 0)

// x and y must be of unsigned type; x and y may be evaluated multiple times
#define bdus_is_positive_multiple_of_(x, y) ((x) != 0 && (x) % (y) == 0)

// x and y must be of unsigned type; x and y may be evaluated multiple times
#define bdus_is_zero_or_multiple_of_(x, y) ((x) == 0 || (x) % (y) == 0)

/* -------------------------------------------------------------------------- */
/* system calls */

int bdus_close_keep_errno_(int fd);

int bdus_open_retry_(const char *pathname, int flags);

int bdus_ioctl_retry_(int fd, unsigned long request);
int bdus_ioctl_arg_retry_(int fd, unsigned long request, void *argp);

// sets error with bdus_set_error_() and returns 0 on failure
size_t bdus_get_page_size_(void);

/* -------------------------------------------------------------------------- */
/* redirection & daemonization */

bool bdus_redirect_to_dev_null_(int fd, int flags);

bool bdus_daemonize_(void);

/* -------------------------------------------------------------------------- */
/* errors */

const char *bdus_get_error_message_(void);

// - 'errno' is set to 'errno_value'.
// - The value of errno and a suitable description are appended to the resulting
//   error message.
// - The error message (including the errno part) is truncated to 1023
//   characters.
void bdus_set_error_(
    int errno_value, const char *error_message_format, ...
    ) __attribute__((format(printf, 2, 3)));

void bdus_set_error_dont_append_errno_(
    int errno_value, const char *error_message_format, ...
    ) __attribute__((format(printf, 2, 3)));

/* -------------------------------------------------------------------------- */

#endif /* LIBBDUS_HEADER_UTILITIES_H_ */
