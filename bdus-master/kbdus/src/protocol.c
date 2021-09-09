/* -------------------------------------------------------------------------- */
/* includes */

#include <kbdus.h>
#include <kbdus/inverter.h>
#include <kbdus/protocol.h>
#include <kbdus/protocols/ioctl.h>
#include <kbdus/protocols/mmap.h>
#include <kbdus/protocols/rw.h>
#include <kbdus/utilities.h>

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/stddef.h>
#include <linux/types.h>

/* -------------------------------------------------------------------------- */
/* private -- constants */

static const struct kbdus_protocol *const kbdus_protocol_all_[] =
{
    &kbdusioctl_protocol,
    &kbdusmmap_protocol,
    &kbdusrw_protocol,
};

/* -------------------------------------------------------------------------- */
/* component init / exit */

int __init kbdus_protocol_init(void)
{
    size_t i;
    const struct kbdus_protocol *protocol;
    int ret;

    // initialize protocols

    for (i = 0; i < ARRAY_SIZE(kbdus_protocol_all_); ++i)
    {
        protocol = kbdus_protocol_all_[i];

        kbdus_assert(
            !protocol->validate_config || protocol->create_instance
            );

        kbdus_assert(
            !!protocol->create_instance == !!protocol->destroy_instance
            );

        if (protocol->init)
        {
            ret = protocol->init();

            if (ret != 0)
            {
                // terminate initialized protocols

                for ( ; i > 0; --i)
                {
                    protocol = kbdus_protocol_all_[i - 1];

                    if (protocol->exit)
                        protocol->exit();
                }

                // return error

                return ret;
            }
        }
    }

    // success

    return 0;
}

void kbdus_protocol_exit(void)
{
    size_t i;
    const struct kbdus_protocol *protocol;

    // terminate protocols

    for (i = ARRAY_SIZE(kbdus_protocol_all_); i > 0; --i)
    {
        protocol = kbdus_protocol_all_[i - 1];

        if (protocol->exit)
            protocol->exit();
    }
}

/* -------------------------------------------------------------------------- */
/* component interface */

const struct kbdus_protocol *kbdus_protocol_lookup(const char *protocol_name)
{
    size_t i;
    const struct kbdus_protocol *protocol;

    // find protocol with given name

    for (i = 0; i < ARRAY_SIZE(kbdus_protocol_all_); ++i)
    {
        if (strcmp(kbdus_protocol_all_[i]->name, protocol_name) == 0)
            return kbdus_protocol_all_[i];
    }

    // no protocol with given name

    return NULL;
}

bool kbdus_protocol_validate_config(
    const struct kbdus_protocol *protocol,
    const struct kbdus_config *config
    )
{
    if (protocol->validate_config)
        return protocol->validate_config(config);
    else
        return true;
}

/* -------------------------------------------------------------------------- */

struct kbdus_protocol_instance *kbdus_protocol_create_instance(
    const struct kbdus_protocol *protocol,
    const struct kbdus_config *config
    )
{
    if (protocol->create_instance)
    {
        return
            (struct kbdus_protocol_instance *)
            protocol->create_instance(config);
    }
    else
    {
        return NULL;
    }
}

void kbdus_protocol_destroy_instance(
    const struct kbdus_protocol *protocol,
    struct kbdus_protocol_instance *instance
    )
{
    if (protocol->destroy_instance)
        protocol->destroy_instance((void *)instance);
}

ssize_t kbdus_protocol_handle_control_read_iter(
    const struct kbdus_protocol *protocol,
    struct kbdus_protocol_instance *instance,
    struct kbdus_inverter *inverter,
    struct iov_iter *to
    )
{
    if (protocol->handle_control_read_iter)
    {
        return protocol->handle_control_read_iter(
            (void *)instance, inverter, to
            );
    }
    else
    {
        return -EINVAL;
    }
}

ssize_t kbdus_protocol_handle_control_write_iter(
    const struct kbdus_protocol *protocol,
    struct kbdus_protocol_instance *instance,
    struct kbdus_inverter *inverter,
    struct iov_iter *from
    )
{
    if (protocol->handle_control_write_iter)
    {
        return protocol->handle_control_write_iter(
            (void *)instance, inverter, from
            );
    }
    else
    {
        return -EINVAL;
    }
}

int kbdus_protocol_handle_control_mmap(
    const struct kbdus_protocol *protocol,
    struct kbdus_protocol_instance *instance,
    struct kbdus_inverter *inverter,
    struct vm_area_struct *vma
    )
{
    if (protocol->handle_control_mmap)
        return protocol->handle_control_mmap((void *)instance, inverter, vma);
    else
        return -EINVAL;
}

int kbdus_protocol_handle_control_ioctl(
    const struct kbdus_protocol *protocol,
    struct kbdus_protocol_instance *instance,
    struct kbdus_inverter *inverter,
    unsigned int command,
    void __user *argument_usrptr
    )
{
    if (protocol->handle_control_ioctl)
    {
        return protocol->handle_control_ioctl(
            (void *)instance, inverter, command, argument_usrptr
            );
    }
    else
    {
        return -ENOTTY;
    }
}

/* -------------------------------------------------------------------------- */
