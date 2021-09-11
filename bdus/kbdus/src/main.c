/* -------------------------------------------------------------------------- */
/* includes */

#include <kbdus.h>
#include <kbdus/config.h>
#include <kbdus/control.h>
#include <kbdus/device.h>
#include <kbdus/inverter.h>
#include <kbdus/protocol.h>
#include <kbdus/utilities.h>

#include <linux/genhd.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/module.h>
#include <linux/stringify.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,13,0)
    #include <linux/build_bug.h>
#else
    #include <linux/bug.h>
#endif

/* -------------------------------------------------------------------------- */
/* module info */

MODULE_LICENSE("GPL");

MODULE_VERSION(
    __stringify(BDUS_VERSION_MAJOR.BDUS_VERSION_MINOR.BDUS_VERSION_PATCH)
    );

MODULE_DESCRIPTION(
    "This module is part of BDUS, a framework for developing Block Devices in"
    " User Space (https://github.com/albertofaria/bdus)."
    );

MODULE_AUTHOR("Alberto Faria <albertofaria.dev@gmail.com>");

/* -------------------------------------------------------------------------- */
/* module init / exit */

static int __init kbdus_init_(void)
{
    int ret;

    // static assertion -- ensure that there exist enough minor numbers for all
    // devices

    BUILD_BUG_ON((KBDUS_HARD_MAX_DEVICES * DISK_MAX_PARTS) > (1 << MINORBITS));

    // static assertion -- ensure that field `disk_name` of `struct gendisk` can
    // hold the longest device name

    BUILD_BUG_ON(
        sizeof("bdus-" __stringify(KBDUS_HARD_MAX_DEVICES)) > DISK_NAME_LEN
        );

    // static assert -- ensure that request & reply structure for protocol
    // "mmap" fits in one cache line

    BUILD_BUG_ON(sizeof(union kbdusmmap_reply_and_request) > 64);

    // static assertion -- ensure that all request & reply structures for
    // protocol "mmap" fit in one page

    BUILD_BUG_ON(KBDUSMMAP_MAX_REQUEST_BUFFERS * 64 > PAGE_SIZE);

    // initialize component "inverter"

    if ((ret = kbdus_inverter_init()) != 0)
        goto error;

    // initialize component "protocol"

    if ((ret = kbdus_protocol_init()) != 0)
        goto error_inverter_exit;

    // initialize component "device"

    if ((ret = kbdus_device_init()) != 0)
        goto error_protocol_exit;

    // initialize component "control"

    if ((ret = kbdus_control_init()) != 0)
        goto error_device_exit;

    // print success message

    kbdus_log_if_debug("Loaded.");

    // success

    return 0;

    // failure

error_device_exit:
    kbdus_device_exit();
error_protocol_exit:
    kbdus_protocol_exit();
error_inverter_exit:
    kbdus_inverter_exit();
error:
    return ret;
}

static void __exit kbdus_exit_(void)
{
    // terminate components

    kbdus_control_exit();
    kbdus_device_exit();
    kbdus_protocol_exit();
    kbdus_inverter_exit();

    // print success message

    kbdus_log_if_debug("Unloaded.");
}

module_init(kbdus_init_);
module_exit(kbdus_exit_);

/* -------------------------------------------------------------------------- */
