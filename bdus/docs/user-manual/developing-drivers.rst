.. .......................................................................... ..

.. _developing-drivers:

Developing drivers
==================

Describes the user-space API made available by the framework for implementing block device drivers, along with the ``bdus-destroy`` user-space tool.

.. .......................................................................... ..

API for writing drivers
-----------------------

- Running description and reference of libbdus' interface.

- Document libbdus' interface as if it were a Python module, in a running text style which incrementally introduces symbols.

- Only ``bdus_run()``, not ``bdus_rerun()``.

- How we specify operations which correspond to the several request types.

- How we also have initialize and terminate.

- How we specify attributes.

- How the attributes as seen from the callbacks may have been adjusted.

- If none of ``write``, ``write_same``, ``write_zeros``, ``fua_write``, ``discard``, and ``secure_erase`` callbacks are implemented, the device will be marked as read-only, meaning that it won't be possible to open it for writing even if you don't intend to change its contents.

- About write_same: If we didn't implement this operation, device_write would be used instead, but this implementation is more efficient as it avoids allocating a bigger request buffer.

- **On attributes, after introducing size and logical and physical block size:** Attributes that limit the size of certain types of requests are also available. You can set these to ensure that requests aren't bigger than the value you choose. Note that this does not limit users of the device, they can always send big requests and the kernel will automatically break them up into smaller requests that satisfy the requirements of your driver.

- **Very cool note:** as the API reference states, the buffers passed to the callbacks are aligned in memory to the system's page size. This means that they are always suitably aligned for use in reads and writes of files opened with ``O_DIRECT``.

.. rubric:: Some old notes

- Documentation and terminology:

  - Uniformize terminology in code documentation.
  - Terms: request, operation, implementation, callback.
  - **Request**: request submitted to the device drivers by an application or by the kernel itself, e.g. request to read data.
  - **Operations**: the driver writer must implement operations. These will satisfy the requests sent to the driver.
  - **Callback**: C function, passed to libbdus, which will be invoked to satisfy a certain type of request. Callbacks implement operations. Also, 'initialize' and 'terminate' callbacks.
  - Operation ~ Callback.

- Some concepts:

  - Notes:
    - Driver: Implements block device behavior.
    - Driver instance: One running instance of a driver, giving rise to a device.
    - Device: A block device whose behavior is defined by one driver instance.
  - Verbs:
    - Initialize / Create device: The driver instance must immediately be able to process requests.
    - Unavailable device: A device that is created but not available to clients.
    - Available / Fully created device: A device that is available to clients.
    - Terminate / Destroy device: A device is terminated when the corresponding driver instance dies. bdus-terminate may be used to safely terminate a device, ensuring data persistency, which is not guaranteed when the driver instance is killed.

.. .......................................................................... ..

How ioctl commands work
-----------------------

- How block device operations are idempotent, and how ioctl commands should be as well, because they may appear to have failed to clients even though the driver successfully processes them, because of signals interrupting system calls and etcetera.

- Some cool text about ioctl commands: https://docs.rs/nix/0.17.0/nix/sys/ioctl/index.html

- By (weird) convention, ``ENOTTY`` should be returned if the *ioctl* command is not recognized (don't know how to handle it).

- ioctl commands should be constructed with the _IO, _IOR, _IOW, or _IOWR macros. They can be decoded if necessary in your driver implementation with the _IOC_DIR, _IOC_TYPE, _IOC_NR, and _IOC_SIZE macros. The size of the argument is never greater than 16383 bytes (16 KiB - 1). Under the `Alpha <https://en.wikipedia.org/wiki/DEC_Alpha>`_, `MIPS <https://en.wikipedia.org/wiki/MIPS_architecture>`_, and `PowerPC <https://en.wikipedia.org/wiki/PowerPC>`_ architectures, the size is actually never greater than 8191 bytes (8 KiB - 1).

- Must document ioctl command format comprehensively; there appears to be no single useful resource (with the exception of kernel documentation) that we can refer users to.

- When the ioctl command is ill-formed (*i.e.*, was not properly constructed using one of the ``_IO``, ``_IOW``, ``_IOR``, or ``_IOWR`` macros) it is automatically rejected and doesn't reach the user-space driver.

- Also, for commands construct with ``_IOW``, ``_IOR``, or ``_IOWR``, an argument is always present.

- The size of the ioctl argument is specified in the definition of its command, and is limited by a system-dependent upper bound, which is 8 KiB - 1 B on Alpha, MIPS, and PowerPC, and 16 KiB - 1 B on other architectures.

.. .......................................................................... ..

Compiling and running the driver
--------------------------------

- (Note that libbdus automatically loads the kbdus kernel module when necessary.)

- Running the driver, driver lifecycle, device vs driver, using ``bdus destroy``, how killing drivers may lead to data loss and how ``sync`` ing the device ensures that written but not yet flushed data is persistently stored.

- Note that drivers linked against a given *libbdus* version will simply fail to load if a compatible version is not installed.

- Flush: Flush OS caches for the device and then submit and await a FLUSH request to the device.

- Killing driver may lead to data loss. Using ``bdus destroy`` is safe.

- OLD NOTE: The typical workflow is as follows:

  - The user runs an executable which implements a BDUS device;
  - The program usually daemonizes itself after the device is created;
  - The program also usually outputs the path to the created device before daemonizing;
  - The user is then able to use the device as they would any other block device;
  - If an error internal to the BDUS device occurs, the device disappears;
  - If the BDUS driver process is killed or otherwise ends execution, the device disappears;
  - If the BDUS driver marked the device as stoppable, the bdus-stop utility may also be used to make the device disappear.

- Device life cycle:

  - Initialize the device.
  - After possibly receiving some operations (because kernel tries to find a partition table in the device), device becomes available (shows up in /dev, TODO: check this), usable by clients
  - Terminate the device; after requesting termination, several operations may still be received (e.g. to persist data in cache and stuff), and after does the device become fully terminated.

- VERY IMPORTANT NOTES:

  - If you compile you program against A.B.C, it will run correctly with any A.B.? version. But if the libbdus installed in the system is upgraded or changed to a version with difference major or minor numbers, it will not run because it will not find libbdus. In summary, a program that links with libbdus will only run if a libbdus version with the same major and minor versions is available. (This is true for all open-source releases, even 0.A.B ones.) But do note that the API is backward compatible whenever the minor (or patch) version is incremented --- just the relinking is necessary.

  - By compiling with -lbdus, the driver will link to the most recent libbdus version installed in the system (or otherwise available in the path where libraries are looked up).

  - By compiling with -lbdus1.2, the driver will link to the most recent patch of the 1.2.X versions of libbdus available in the system.

  - By compiling with -lbdus1.2.3, the driver will link to exactly that version, **but when running, the most recent 1.2.X version available in the system will be used!**

  - These things are a consequence of the way Linux handles dynamic library versioning and of the fact (and conscious decision) that ABI compatibility is only guaranteed between versions with the same major and minor numbers. API compatibility, on the other hand, is what you would expect: full backward compatibility between versions with the same major version.

.. .......................................................................... ..

API for managing existing drivers programmatically
--------------------------------------------------

- Compare with previously described ``bdus destroy``.

.. .......................................................................... ..

API for replacing drivers
-------------------------

- *I.e.*, ``bdus_rerun()``.

- Also how everything fails if any step fails.

- How this is transparent to clients, which should not notice in any way that the driver was replaced (timeouts aside, although the default request timeout is 30 seconds).

- An example of a "replaceable" driver is :repo-file:`examples/zero.c`.

- Actually, you can replace any driver, but you must before make sure that block device semantics won't be violated!, e.g., such as completed writes having to be visible. In the examples, the ram drivers aren't replaceable (because they lose their contents, the second driver just starts with unspecified content) but zero is replaceable (because its content is always the same) and loop is too (because its content comes from an underlying device and the driver doesn't store any other state).

.. .......................................................................... ..

Creating recoverable drivers
----------------------------

- Restating that all operations should be idempotent, including ioctl calls, as they may be retried by the new driver. Also how a lot of care must be taken to ensure that the driver can restart from *any* state that the failed driver may have left *and still fulfill all block device semantics, not only crash-semantics*, because for the client the device never failed.

- How to make the example drivers recoverable, and which can be made recoverable without breaking block device semantics, and why.

- Note that the only two examples that can be made recoverable without any implementation change is zero (because it does not support any operation that changes its content) and loop (because the device contents are always entirely stored in an underlying device the driver doesn't need to persist any special state between executions).

- In the cases where semantics would be preserved, the only thing that you have to do to make the driver recoverable is set attribute ``recoverable`` to ``true``. This will make it so if the driver terminates without anyone asking to destroy the device, the device will continue to exist so that another driver can take the failed driver's place.

.. .......................................................................... ..

Other considerations
--------------------

- Can't be used as swap.

- BDUS devices should not be used for swap, as the driver is a user process and as such it can be swapped. What would happen if the OS tried to swap out the driver process to its own device? Dead lock I guess, or timeout on requests somehow.

.. .......................................................................... ..

Driver hot swapping and recovery
--------------------------------

Support for hot-swapping (upgrading) drivers, and to recover from crashed drivers.

The driver for an existing BDUS device can be replaced.
The driver that replaces the existing driver should use the ``bdus_rerun()`` function and specify the index of the device whose driver to replace.
To use this function, specify operations and attributes as normal.
But libbdus will check if these attributes are compatible with those of the existing device, and the same (or more) request types must be supported.

One can also allow for a BDUS device to keep on existing after its driver fails.
For this, set the ``recoverable`` attribute to ``true``.
This will make it so the device is not destroyed if its driver terminates abnormally.
(Note that, regardless of the value of this attribute, the device is destroyed if someone asked for the driver to terminate and it terminates successfully.)
(Also note that, regardless of the value of this attribute, the device is destroyed if its driver fails before the device becomes available to clients.)
A driver can the attach to the existing device with the ``bdus_rerun()`` function, just like for hot swapping.

In either of these cases (driver hot swapping or recovery), no client requests are lost, *i.e.*, if they are not satisfied by the driver that failed, they will be (re)sent to the driver that replaces the first.
It may also happen that the same be request be sent to both drivers (only if the first driver terminated abnormally!!).
This should be okay, as block device operations are idempotent.
But make sure to make them idempotent.
Don't forget that ioctl requests should also be idempotent!

There is actually a race condition when using ``bdus_rerun()``, because the index that the caller specifies may be attributed to a new device even while ``bdus_rerun()`` is being invoked. The only resilient solution would be to identify the device by its seqnum, but we currently hide the seqnum from the libbdus user.

**For proper driver recovery, ioctl requests should be idempotent, just like all other requests!**

There are other considerations as well:

- Regarding driver recovery, for block device semantics to be respected, it should be the case that the recovering driver can always recover the state of the failed driver without violating any guarantees such as writes being visible. If this is not possible, your device should never change drivers, and it should just be destroyed upon driver failure. The device can come back later, but writes before flushes don't need to be visible in this case --- this situation is similar to a machine crash and subsequent reboot.

- It may be impossible to destroy a block device without a controlling driver without loosing data, as the final flush request can only be processed if the device has an attached driver (and is not read-only). The ``bdus terminate`` also freezes until the request times out in this case, which can be taken to be a bug by users.

**Document that allowing device to exist after driver failure is very tricky: one must ensure that, no matter how the previous driver terminated (successful, terminate() failed, sudden crash, etc.), the new driver must be able to function an fully obey block device semantics, like not forgetting writes and stuff. Basically, whenever the device keeps on existing, "crash" semantics (i.e., only flushed writes are guaranteed to be remembered) are not sufficient --- to the client, it must absolutely look like if the original driver never failed (timeouts aside while no driver is controlling the device).**

Also, when a driver is terminating because another driver is trying to attach to the device, the driver receives a flush request immediately before being terminated. This may come in handy for some drivers, idk.

**This is now wrong:** "Note that block device semantics mean that they are resilient to failures, in that clients can ensure that data is persistently stored. I.e., they should be able to fail at arbitrary instants in time and be able to recover without violating interface semantics. This is why your terminate() shouldn't do anything critical. Your device should continue working by restarting the driver even if terminate() is not run. So that is why you can't return errors from terminate(). Nothing that can fail should be done there."

.. .......................................................................... ..

SOME COOL EXAMPLES
------------------

These are some cool examples of copying data to a BDUS block device using ``dd``.
They illustrate the semantics of the Linux page cache and issues of cache bypassing and data persistence.
Note that these results are applicable to any block device, not only BDUS devices.

.. rubric:: The device

We use a simple loop device that simply mirrors an underlying block device.
The internal functioning of the driver is not actually relevant, only the fact that it supports *read*, *write*, and *flush* requests (among others).
Also, the device's logical and physical block sizes are both 512 bytes.
Also, the device is ``/dev/bdus-0``.

.. rubric:: Scenario 1

A simple copy, no special flags.

Command: ``dd if=/dev/zero of=/dev/bdus-0 count=16 bs=1024``

Driver log::

    log: thread 15: read(0x7ffff759c000, 0, 4096, dev)
    log: thread 15: read(0x7ffff759c000, 4096, 4096, dev)
    log: thread 15: read(0x7ffff759c000, 8192, 4096, dev)
    log: thread 15: read(0x7ffff759c000, 12288, 4096, dev)
    log: thread 15: write(0x7ffff759c000, 0, 16384, dev)

As no special flags are given to ``dd``, it does a regular ``open()`` on the BDUS device.
This means that the page cache is used, and it buffers writes.
Note that the page cache granularity is of 4 KiB in this machine (*i.e.*, the page cache stores 4 KiB blocks).

Note also that ``dd`` is performing 16 sequential writes of 1024 bytes each.
Thus, for every 4th write, the page cache reads a whole page from disk (4096 bytes) and then modifies its first 1024 bytes with the written data.
The remaining writes simply modify remaining page data which has already been read to the cache.

Note that the block device does not receive any actual *write* requests until ``dd`` finishes.
This is because the page cache has a write-back policy, and dirty pages in the page cache are converted into *write* requests to the device only when the file is closed.
In fact, the only *write* request received by the device is of size 16 KiB.
(Dirty pages could also be flushed prior to closing the file if the page cache were at limit capacity.)

But note that the device does not receive any *flush* requests, and as such the data might not be persistently stored!
If necessary, an ``fsync()`` or ``fdatasync()`` could be performed on the device to ensure that all previously written data is persistently stored.

.. rubric:: Scenario 2

A simple copy, but using ``O_DIRECT``.

Command: ``dd if=/dev/zero of=/dev/bdus-0 count=16 bs=1024 oflags=direct``

Driver log::

    log: thread 15: write(0x7ffff759c000, 0, 1024, dev)
    log: thread 15: write(0x7ffff759c000, 1024, 1024, dev)
    log: thread 15: write(0x7ffff759c000, 2048, 1024, dev)
    log: thread 15: write(0x7ffff759c000, 3072, 1024, dev)
    log: thread 15: write(0x7ffff759c000, 4096, 1024, dev)
    log: thread 15: write(0x7ffff759c000, 5120, 1024, dev)
    log: thread 15: write(0x7ffff759c000, 6144, 1024, dev)
    log: thread 15: write(0x7ffff759c000, 7168, 1024, dev)
    log: thread 15: write(0x7ffff759c000, 8192, 1024, dev)
    log: thread 15: write(0x7ffff759c000, 9216, 1024, dev)
    log: thread 15: write(0x7ffff759c000, 10240, 1024, dev)
    log: thread 15: write(0x7ffff759c000, 11264, 1024, dev)
    log: thread 15: write(0x7ffff759c000, 12288, 1024, dev)
    log: thread 15: write(0x7ffff759c000, 13312, 1024, dev)
    log: thread 15: write(0x7ffff759c000, 14336, 1024, dev)
    log: thread 15: write(0x7ffff759c000, 15360, 1024, dev)

Here, the ``O_DIRECT`` flag is given to ``open()``, which causes all reads and writes through the returned file descriptor to bypass the page cache.
As such, every 1024 byte write is directly transformed into a *write* request to the device.

As before, no *flush* request is sent to the device, though.

.. rubric:: Scenario 3

A simple copy, but using ``O_SYNC``.

Command: ``dd if=/dev/zero of=/dev/bdus-0 count=16 bs=1024 oflags=sync``

Driver log::

    log: thread 15: read(0x7ffff759c000, 0, 4096, dev)
    log: thread 15: write(0x7ffff759c000, 0, 4096, dev)
    log: thread 15: flush(dev)
    log: thread 15: write(0x7ffff759c000, 0, 4096, dev)
    log: thread 15: flush(dev)
    log: thread 15: write(0x7ffff759c000, 0, 4096, dev)
    log: thread 15: flush(dev)
    log: thread 15: write(0x7ffff759c000, 0, 4096, dev)
    log: thread 15: flush(dev)
    log: thread 15: read(0x7ffff759c000, 4096, 4096, dev)
    log: thread 15: write(0x7ffff759c000, 4096, 4096, dev)
    log: thread 15: flush(dev)
    log: thread 15: write(0x7ffff759c000, 4096, 4096, dev)
    log: thread 15: flush(dev)
    log: thread 15: write(0x7ffff759c000, 4096, 4096, dev)
    log: thread 15: flush(dev)
    log: thread 15: write(0x7ffff759c000, 4096, 4096, dev)
    log: thread 15: flush(dev)
    log: thread 15: read(0x7ffff759c000, 8192, 4096, dev)
    log: thread 15: write(0x7ffff759c000, 8192, 4096, dev)
    log: thread 15: flush(dev)
    log: thread 15: write(0x7ffff759c000, 8192, 4096, dev)
    log: thread 15: flush(dev)
    log: thread 15: write(0x7ffff759c000, 8192, 4096, dev)
    log: thread 15: flush(dev)
    log: thread 15: write(0x7ffff759c000, 8192, 4096, dev)
    log: thread 15: flush(dev)
    log: thread 15: read(0x7ffff759c000, 12288, 4096, dev)
    log: thread 15: write(0x7ffff759c000, 12288, 4096, dev)
    log: thread 15: flush(dev)
    log: thread 15: write(0x7ffff759c000, 12288, 4096, dev)
    log: thread 15: flush(dev)
    log: thread 15: write(0x7ffff759c000, 12288, 4096, dev)
    log: thread 15: flush(dev)
    log: thread 15: write(0x7ffff759c000, 12288, 4096, dev)
    log: thread 15: flush(dev)

Like scenario 1, this uses the page cache.
However, because ``O_SYNC`` is given to ``open()``, writes only return after the data is persistently stored.

The same thing with reading to fill the page cache happens with every 4th write, but unlike for scenario 1, after each write modifies the corresponding page in the page cache, the whole page is written to the device through a *write* request (because the page cache works this way).
Further, after every *write* request, a *flush* request is processed, in order to ensure that the written data is persistently stored.

Curiously, if the driver also supports *FUA write* requests, nothing will change, *i.e.*, *write*-*flush* request pairs will still be used.

.. rubric:: Scenario 4

A simple copy, but using both ``O_DIRECT`` and ``O_SYNC``.

Command: ``dd if=/dev/zero of=/dev/bdus-0 count=16 bs=1024 oflags=direct,sync``

Driver log::

    log: thread 15: write(0x7ffff759c000, 0, 1024, dev)
    log: thread 15: flush(dev)
    log: thread 15: flush(dev)
    log: thread 15: write(0x7ffff759c000, 1024, 1024, dev)
    log: thread 00: flush(dev)
    log: thread 00: flush(dev)
    log: thread 00: write(0x7ffff73bc000, 2048, 1024, dev)
    log: thread 00: flush(dev)
    log: thread 00: flush(dev)
    log: thread 00: write(0x7ffff73bc000, 3072, 1024, dev)
    log: thread 00: flush(dev)
    log: thread 00: flush(dev)
    log: thread 00: write(0x7ffff73bc000, 4096, 1024, dev)
    log: thread 00: flush(dev)
    log: thread 00: flush(dev)
    log: thread 00: write(0x7ffff73bc000, 5120, 1024, dev)
    log: thread 00: flush(dev)
    log: thread 00: flush(dev)
    log: thread 00: write(0x7ffff73bc000, 6144, 1024, dev)
    log: thread 00: flush(dev)
    log: thread 00: flush(dev)
    log: thread 00: write(0x7ffff73bc000, 7168, 1024, dev)
    log: thread 00: flush(dev)
    log: thread 00: flush(dev)
    log: thread 00: write(0x7ffff73bc000, 8192, 1024, dev)
    log: thread 00: flush(dev)
    log: thread 00: flush(dev)
    log: thread 00: write(0x7ffff73bc000, 9216, 1024, dev)
    log: thread 00: flush(dev)
    log: thread 00: flush(dev)
    log: thread 00: write(0x7ffff73bc000, 10240, 1024, dev)
    log: thread 00: flush(dev)
    log: thread 00: flush(dev)
    log: thread 00: write(0x7ffff73bc000, 11264, 1024, dev)
    log: thread 00: flush(dev)
    log: thread 00: flush(dev)
    log: thread 00: write(0x7ffff73bc000, 12288, 1024, dev)
    log: thread 00: flush(dev)
    log: thread 00: flush(dev)
    log: thread 00: write(0x7ffff73bc000, 13312, 1024, dev)
    log: thread 00: flush(dev)
    log: thread 00: flush(dev)
    log: thread 00: write(0x7ffff73bc000, 14336, 1024, dev)
    log: thread 00: flush(dev)
    log: thread 00: flush(dev)
    log: thread 00: write(0x7ffff73bc000, 15360, 1024, dev)
    log: thread 00: flush(dev)
    log: thread 00: flush(dev)

Same as scenario 2 with only ``O_DIRECT``, but a *flush* request is sent to the driver after every *write* request to ensure that previously written data is persistently stored.

No idea why **two** *flush* requests are sent, though :/

Curiously, if the driver also supports *FUA write* requests, the *write*-*flush* request pairs will turn into single *FUA write*, but the second flush is still performed::

    log: thread 15: fua_write(0x7ffff759c000, 0, 1024, dev)
    log: thread 15: flush(dev)
    log: thread 15: fua_write(0x7ffff759c000, 1024, 1024, dev)
    log: thread 15: flush(dev)
    log: thread 15: fua_write(0x7ffff759c000, 2048, 1024, dev)
    log: thread 15: flush(dev)
    log: thread 15: fua_write(0x7ffff759c000, 3072, 1024, dev)
    log: thread 15: flush(dev)
    log: thread 15: fua_write(0x7ffff759c000, 4096, 1024, dev)
    log: thread 15: flush(dev)
    log: thread 15: fua_write(0x7ffff759c000, 5120, 1024, dev)
    log: thread 15: flush(dev)
    log: thread 15: fua_write(0x7ffff759c000, 6144, 1024, dev)
    log: thread 15: flush(dev)
    log: thread 15: fua_write(0x7ffff759c000, 7168, 1024, dev)
    log: thread 15: flush(dev)
    log: thread 15: fua_write(0x7ffff759c000, 8192, 1024, dev)
    log: thread 15: flush(dev)
    log: thread 15: fua_write(0x7ffff759c000, 9216, 1024, dev)
    log: thread 15: flush(dev)
    log: thread 15: fua_write(0x7ffff759c000, 10240, 1024, dev)
    log: thread 15: flush(dev)
    log: thread 15: fua_write(0x7ffff759c000, 11264, 1024, dev)
    log: thread 15: flush(dev)
    log: thread 15: fua_write(0x7ffff759c000, 12288, 1024, dev)
    log: thread 15: flush(dev)
    log: thread 15: fua_write(0x7ffff759c000, 13312, 1024, dev)
    log: thread 15: flush(dev)
    log: thread 15: fua_write(0x7ffff759c000, 14336, 1024, dev)
    log: thread 15: flush(dev)
    log: thread 15: fua_write(0x7ffff759c000, 15360, 1024, dev)
    log: thread 15: flush(dev)

.. .......................................................................... ..
