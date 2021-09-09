.. .......................................................................... ..

.. _future-work:

Future work
===========

This section lists several ideas and possible improvements for the BDUS framework.

.. This section contains a :ref:`TODO list <todo-list>` for the BDUS framework, as well as some possibly :ref:`interesting ideas <ideas>` for its development.

.. .......................................................................... ..

Pre-open-source TODO list and ideas
-----------------------------------

The road to open-source
-----------------------

Documentation:

- Cleanup kbdus' interface and its documentation.
- Cleanup libbdus' interface and its documentation.
- Get the User Manual to a publishable state.
- Get the Developer Manual to a publishable state.

Code:

- Final commit in closed-source repo, ready to be the first commit in the open-source repo.
- libbdus' soname becomes MAJOR.MINOR, allowing drivers and command ``bdus`` to use libbdus versions with different patch versions.
- Modify libbdus to allow for kbdus versions with different patch versions.

Meta:

- Sort out the licensing.

Interface improvements
~~~~~~~~~~~~~~~~~~~~~~

- Rename ``bdus_rerun()`` to something that looks less like ``bdus_run()``?

- Rename ``struct bdus_dev`` to ``struct bdus_ctx`` or ``struct bdus_context``?

- Replace occurrences of ``_dev`` in public API to ``_device``?

- Would it make more sense if ``bdus_rerun()`` waited until the existing device becomes available if it isn't already, instead of failing like it does currently?

- Maybe add command ``bdus destroy-all [--no-flush]``, which terminates all existing BDUS devices. (Possible implementation hints: Expose hard maximum number of devices through ``kbdus.h``, create *ioctl* command for fetching seqnums of all existing devices, and use existing flush and termination *ioctl* commands for the implementation. Would we be able to keep the ``bdus`` command dumb this way, though?)

- ``bdus destroy`` hangs if no driver attached because flush can't be served. How can we make this more intuitive? Should we output things like "Flushing... Terminating... Done."? Should we display a message saying that flush is taking a bit to complete, or driver is taking a bit to terminate, if more than 2 seconds elapse? Only on terminal mode and remove the message when things finally finish successfully?

- Should ``bdus destroy`` output things like "Flushing... Terminating... Done."?

Implementation improvements
~~~~~~~~~~~~~~~~~~~~~~~~~~~

- Terminology:

  - Uniformize names across kbdus, libbdus, cmdbdus: device, driver, session, etc.

  - Uniformize verbs across kbdus, libbdus, cmdbdus: destroy device, terminate driver, terminate session, attach to device, flush, etc.

  - Should the term "driver" be used in kbdus' context at all?

- Should probably allow more than 64 concurrent threads with protocol "mmap".

Performance
~~~~~~~~~~~

- Insert some more profiling code in kbdus and also libbdus and create a proper way to extract those results from user-space. (Although using existing black-box profiling and tracing tools would be better, having some kind of profiling infrastructure in both kbdus and libbdus could be interesting. libbdus could expose a (probably private) interface to obtain profiling data, which could inform the user of how much time is spent in each task or something.)

Documentation
~~~~~~~~~~~~~

- Navigation bar doesn't fit even with scrolling if version warning banner is visible.

- Document examples a bit more, especially what makes zero "replaceable" and loop "recoverable".

- Document somewhere that increasing concurrency also increases memory usage.

- Properly and thoroughly document kbdus' internal APIs (document protocol interface, then check caller for contract violations, then document device interface, etc.).

- We checked, and readv(), writev(), and ioctl() can all run concurrently with themselves and with each other on /dev/bdus-control (character device).

- Also, we checked, and ioctl() can run concurrently with itself on /dev/bdus-0 (block device).

- Should put links in API reference symbols docs to relevant sections of the User Manual.

- ``initialize()`` & ``terminate()`` are useful because they are only run after arguments are checked and device creation is started. Also, initialize() only runs after previous driver terminates when replacing drivers.

- The Linux kernel only supports "write zeros" requests since Linux 4.10.0, so your driver will never receive write zero requests on older kernel versions (it will receive equivalent write or write_same requests). It's okay to implement the ``write_zeros()`` callback either way, though.

- If flush is not implemented, all operations must immediately persist all effects. If flush is implemented, only it (and write_fua if also implemented) has to persist things. While things are not explicitly persisted, content "on disk" may be in either of the old or new states (or maybe even an unspecified state, I guess).

- Must really specify the block device interface formally (including behavior under mmap).

.. .......................................................................... ..

Things that should most likely be done
--------------------------------------

New features
~~~~~~~~~~~~

- Add support for splicing: could be nice for, *e.g.*, deduplication, where the driver would still have to look at content for writes but for read it could just splice the data from the underlying store.

- Add an asynchronous driver development interface (basically, an asynchronous version of ``bdus_ops``).

- Add support for implementing drivers exporting zoned block devices.

- Add support for more device attributes (*e.g.*, discard alignment).

Implementation improvements
~~~~~~~~~~~~~~~~~~~~~~~~~~~

- The ``char *buffer`` parameter to read and write callbacks aliases other pointers (we checked the assembly). This can prevent some compiler optimizations, as it can think that, *e.g.*, ``buffer`` aliases data accessed through ``dev`` or ``dev->user_data``. Switching the parameter type to ``void *`` and casting its uses to ``char *`` when necessary does not appear to solve the issue. Adding ``restrict`` to the ``buffer`` parameter also is not appropriate, as it tells the compiler that the data accessible through the pointer *is not accessed in any other manner*, which might not be true. Is there any good solution to this problem? Is this a significant problem at all?

- ``rmmod`` immediately after ``bdus destroy`` can fail due to ``kbdus`` still being in use because ``bdus destroy`` is notified of device destruction from ``close()`` of ``/dev/bdus-control``, which is a tiny bit before ``/dev/bdus-control`` is fully closed. How can this be corrected?

- How could we forbid BDUS devices from being used as swap? (BDUS devices should not be used as swap because deadlock if the driver is swapped out to the device that it controls --- driver would have to process request in order to swap itself in.) NBD has the same issue and ``nbd-client`` provides a ``-s`` flag that "attempts to prevent deadlocks by performing mlockall() and adjusting the oom-killer score at an appropriate time." Also, "it does not however guarantee that such deadlocks can be avoided." So BDUS should probably not even try. Probably just document this clearly somewhere.

- Should we really assume device paths to be of the form :samp:`/dev/bdus-{N}`? What if *udev* is not being used? Is this even possible? Research this.

.. .......................................................................... ..

Things that are possibly interesting
------------------------------------

- Maybe extend ioctl request functionality to allow deep copying of argument structures (something like what FUSE does), and maybe also to not require well-formed ioctl commands?

Installation and deployment
~~~~~~~~~~~~~~~~~~~~~~~~~~~

- Consider using DKMS (\https://help.ubuntu.com/community/DKMS) (if the user requests it or by default), if it is usually preinstalled with most Linux distributions. (Or maybe try to install with DKMS by default and fallback to the simple installation?)

Tests
~~~~~

- Create a proper testing infrastructure with lots of functional system tests and unit tests.

- Folder with test definitions, folder with infrastructure to run tests directly on the machine, folder with infrastructure to run tests with vagrant on any given kernel version.

- Setup testing with development-configured kernels, with things like Kasan and lockdep enabled.

- Test that consistently tests the case in which a driver receives a request, then fails, then another driver substitutes it, then it receives the same request, then it serves it.

- Run some tests with valgrind or something to check for illegal access, double free, and memory leak errors.

- Stress tests should also issue ioctl calls and terminate the device before the fio workload finishes.

Protocols and backends
~~~~~~~~~~~~~~~~~~~~~~

- Optimizations to existing protocols and backends:

  - Polling of some sort (see the `white paper on io_uring <https://kernel.dk/io_uring.pdf>`_ for ideas). Polling with fallback to sleeping may be a good optimization for both throughput and latency.

  - Dynamically growing and shrinking the number of threads according to utilization would be nice.

  - Dynamically allocating/deallocating per-thread buffers (and mapping/unmapping memory regions) would also be nice to decrease memory utilization.

  - Protocol "ioctl": Should we try to map bio pages to the user-space driver if all of them are complete (i.e., the full 4 KiB are to be mapped)? This would probably be a common case and would avoid extra memory copying in this case. This should probably be implemented after the optimization in which user-space driver buffers are pre-mapped at device creation.

- Other user-kernel communication interfaces:

  - Communicate over ``read()`` and ``write()`` on ``/dev/bdus-control`` (AFAIK, no reason to be faster than communicating over ``ioctl()``).

  - Communicate over Netlink (https://mdlayher.com/blog/linux-netlink-and-go-part-1-netlink/, \https://people.netfilter.org/pablo/netlink/netlink-libmnl-manual.pdf, \https://people.netfilter.org/pablo/netlink/netlink.pdf, https://wiki.linuxfoundation.org/networking/generic_netlink_howto, https://onlinelibrary.wiley.com/doi/abs/10.1002/spe.981).

  - Communicate over Unix Domain Sockets (would basically become NBD).

  - Communicate over the "relay interface": https://elixir.bootlin.com/linux/latest/source/Documentation/filesystems/relay.rst (this might not make sense at all).

  - Communicate over "POSIX message queues": http://man7.org/linux/man-pages/man7/mq_overview.7.html.

- Other ideas:

  - Should maybe also try a protocol with one blk-mq "hardware" queue per driver thread and the blk-mq request handler (``blk_mq_ops::queue_rq``) blocks waiting for some user-space thread to retrieve and satisfy the request (see flag ``BLK_MQ_F_BLOCKING``). Does this make sense? Or would the request handlers block for too long? Is this abusing the ``BLK_MQ_F_BLOCKING`` flag?

Style
~~~~~

- *kbdus* should conform to the `Linux kernel coding style <https://www.kernel.org/doc/html/latest/process/coding-style.html>`_.

- *kbdus* should conform to the `Linux kernel documentation format <https://www.kernel.org/doc/html/v4.10/doc-guide/kernel-doc.html#writing-kernel-doc-comments>`_.

.. .......................................................................... ..
