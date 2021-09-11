/* -------------------------------------------------------------------------- */
/* includes */

#include <kbdus/utilities.h>

#include <linux/completion.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/limits.h>
#include <linux/spinlock.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,7,0)
    #include <linux/swait.h>
#else
    #include <linux/wait.h>
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0)
    #include <linux/sched/debug.h>
    #include <linux/sched/signal.h>
    #include <linux/sched/task.h>
#else
    #include <linux/sched.h>
#endif

/* -------------------------------------------------------------------------- */
/* miscellaneous */

int kbdus_list_length(const struct list_head *head)
{
    int len;
    struct list_head *iter;

    len = 0;

    list_for_each(iter, head)
    {
        len += 1;
    }

    return len;
}

/* -------------------------------------------------------------------------- */
/* hacks */

// TODO: make this much less hacky and susceptible to completion implementation
// changes!

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,7,0)

static void kbdus_prepare_to_swait_(
    struct swait_queue_head *q, struct swait_queue *wait
    )
{
    wait->task = current;

    if (list_empty(&wait->task_list))
        list_add(&wait->task_list, &q->task_list);
}

static void kbdus_finish_swait_(
    struct swait_queue_head *q, struct swait_queue *wait
    )
{
    __set_current_state(TASK_RUNNING);

    if (!list_empty(&wait->task_list))
        list_del_init(&wait->task_list);
}

#endif

static long __sched kbdus_do_wait_for_common_(
    struct completion *x, long (*action)(long), long timeout, int state
    )
{
    if (!x->done)
    {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,7,0)
        DECLARE_SWAITQUEUE(wait);
#else
        DECLARE_WAITQUEUE(wait, current);
        __add_wait_queue_exclusive(&x->wait, &wait);
#endif

        do
        {
            if (signal_pending_state(state, current))
            {
                timeout = -ERESTARTSYS;
                break;
            }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,7,0)
            kbdus_prepare_to_swait_(&x->wait, &wait);
#endif

            __set_current_state(state);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,7,0)
            raw_spin_unlock_irq(&x->wait.lock);
#else
            spin_unlock_irq(&x->wait.lock);
#endif

            timeout = action(timeout);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,7,0)
            raw_spin_lock_irq(&x->wait.lock);
#else
            spin_lock_irq(&x->wait.lock);
#endif
        }
        while (!x->done && timeout);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,7,0)
        kbdus_finish_swait_(&x->wait, &wait);
#else
        __remove_wait_queue(&x->wait, &wait);
#endif

        if (!x->done)
            return timeout;
    }

    if (x->done != UINT_MAX)
        x->done--;

    return timeout ?: 1;
}

static long __sched kbdus_wait_for_common_(
    struct completion *x, long (*action)(long), long timeout, int state
    )
{
    might_sleep();

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,7,0)
    raw_spin_lock_irq(&x->wait.lock);
#else
    spin_lock_irq(&x->wait.lock);
#endif

    timeout = kbdus_do_wait_for_common_(
        x, action, timeout, state
        );

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,7,0)
    raw_spin_unlock_irq(&x->wait.lock);
#else
    spin_unlock_irq(&x->wait.lock);
#endif

    return timeout;
}

int __sched kbdus_wait_for_completion_interruptible_lifo(struct completion *x)
{
    long t;

    t = kbdus_wait_for_common_(
        x, schedule_timeout, MAX_SCHEDULE_TIMEOUT, TASK_INTERRUPTIBLE
        );

    return t == -ERESTARTSYS ? t : 0;
}

/* -------------------------------------------------------------------------- */
