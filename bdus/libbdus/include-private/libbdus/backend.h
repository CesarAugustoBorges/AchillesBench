#ifndef LIBBDUS_HEADER_BACKEND_H_
#define LIBBDUS_HEADER_BACKEND_H_

/* -------------------------------------------------------------------------- */
/* includes */

#include <bdus.h>

#include <kbdus.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

/* -------------------------------------------------------------------------- */
/* interface for bdus.c */

struct bdus_backend_
{
    // Must be at most 31 characters-long (excluding the null-terminator).
    const char *backend_name;

    // Must be at most 31 characters-long (excluding the null-terminator).
    const char *protocol_name;

    bool (*run)(
        int control_fd,
        uint64_t dev_seqnum,
        struct bdus_dev *dev,
        const struct bdus_internal_config_ *config
        );
};

const struct bdus_backend_ *bdus_backend_lookup_(const char *backend_name);

/* -------------------------------------------------------------------------- */
/* utilities for backend implementations */

bool bdus_backend_on_device_available_(struct bdus_dev *dev, int thread_index);

ssize_t bdus_backend_process_request_(
    struct bdus_dev *dev,
    int thread_index,
    void *payload_buffer,
    uint32_t type,
    uint64_t arg64,
    uint32_t arg32,
    int32_t *out_errno
    );

ssize_t bdus_backend_process_flush_request_(
    struct bdus_dev *dev,
    int thread_index,
    int32_t *out_negated_errno
    );

/* -------------------------------------------------------------------------- */

#endif /* LIBBDUS_HEADER_BACKEND_H_ */
