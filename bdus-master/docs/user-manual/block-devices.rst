.. .......................................................................... ..

.. _block-devices:

Block devices
=============

This section overviews the Linux block device abstraction and details some aspects of its internal operation that those wishing to develop drivers with BDUS should be familiar with.

You may want to read the :ref:`quick-start-guide` before delving into this section.

.. .......................................................................... ..

The block device abstraction
----------------------------

As explained in the :ref:`quick-start-guide`, block devices provide access to storage systems or hardware that present data as a linear sequence of randomly accessible blocks, hiding the intricacies of their operation by exposing a uniform interface.

An application may, for instance, open a block device with the ``open()`` system call and subsequently ``read()`` from and ``write()`` to it, irrespective of the device's underlying implementation.
The Linux kernel itself can also make use of block devices (*e.g.*, as the backing store for a local file system), although the kernel interfaces used to access them are distinct from those available to user-space programs.

Either way, operations on block devices are translated into *requests* and sent to the corresponding device driver, which must then process them appropriately and send a response to the client that submitted them.
These requests may be of several *types*, encoding the action that has been requested (*e.g.*, reading data, writing data, discarding previously written data).
A block device may or may not support each type of request.

Besides the types of request that it supports, each block device is also characterized by some *attributes*, the most obvious being its *size* --- the number of bytes that the device can store.
Two other relevant attributes are the *logical block size* --- the lowest possible size that the device is able to address --- and the *physical block size* --- the lowest possible size that the device can write atomically.
Yet another example is the maximum size of read or write requests that the driver is able to process.

However, with the natural exception of the device size, users need not in general concern themselves with these attributes, as the Linux kernel transparently adjusts requests so that the appropriate requirements are met and the driver can process them (*e.g.*, by splitting an operation into two or more requests).

.. .......................................................................... ..

Request types
-------------

We now describe the semantics of several block device request types.

.. rubric:: Simple data transfer requests

The two most basic types of request that a driver can receive are **read** and **write** requests.
As expected, these are used to read data from, and write data to, the device.
These requests are generated when an application uses the ``read()`` and ``write()`` system calls on a block device, for instance.

However, *write* requests are not the only manner of transferring data to the device: **write same** requests can be used to write the same data to several contiguous logical blocks of the device.
When applicable, this avoids the overhead of creating and filling large data buffers or submitting many smaller requests.

An even more specialized variation of the *write* request is the **write zeros** request, which is used to fill a range of the device with zeros.
User-space applications can submit by performing an ``ioctl()`` call with the ``BLKZEROOUT`` command on the block device.
(Using this ioctl makes the request NOUNMAP, thus the may_unmap argument of the write_zeros() callback will be false.)

If *write same* or *write zeros* requests are submitted to a driver that does not support them, they are transparently converted by the Linux kernel into equivalent *write* requests.

.. rubric:: Cache management requests

Many storage devices feature built-in volatile caches.
Although this improves performance, it may also lead to crash-resilience issues if the cache has a *write back* policy, meaning that data written to the cache is not immediately persisted.
Applications must therefore take appropriate measures if they wish to guarantee that data has been safely stored.

One such measure is the **flush** request, which causes all data written previously to the device to be fully persisted and only returns once this is accomplished.
One way of submitting *flush* requests is by invoking the ``fsync()`` system call on the block device.

However, if the user need only ensure that a specific write operation has been persisted, then issuing a *Force Unit Access* write, or **FUA write** request, is a more appropriate option.
This type of request behaves like a regular *write* request, but only completes when the written data is safely persisted on the device.
Compared to a *flush* request, it avoids the overhead of flushing the entire cache.

If a block device driver supports *flush* requests but not *FUA write* requests, the latter will be transparently converted into a sequence of a *write* request followed by a *flush* request.
However, if both *flush* and *FUA write* requests are not supported by the driver, it is assumed that the device does not feature an internal write-back cache.

.. rubric:: Data discarding requests

- Document that after discard or secure erase, reading those regions will return unspecified data.

We also have discard, which just tells the driver that we don't care about a
certain region of data. This is relevant for SSDs, for example. Reading a
discarded region of the disk gives you unspecified data. Must write before
reading for well defined results.

We also have secure erase requests, which are the same as
discard but guarantee that later reads never get the data that was previously
there.

.. rubric:: *ioctl* requests

Block devices can also process receive ioctl() calls. Strictly speaking, these
are not requests, but we just put them into the same bucket to simplify the
discussion.

Note that strictly speaking, ``ioctl()`` calls do not translate to block device requests.
However, for simplicity of presentation, and for the purposes of the BDUS framework, these are also referred to as requests.

.. rubric:: Summary

In summary, block device drivers may receive requests of the following types:

#. *Read* -- read data from the device;
#. *Write* -- write arbitrary data to the device;
#. *Write same* -- write the same data to several contiguous logical blocks of the device;
#. *Write zeros* -- write zeros to a range of the device;
#. *FUA write* -- write arbitrary data to the device and immediately persist it;
#. *Flush* -- persist all previously written data;
#. *Discard* -- discard a range of data from the device;
#. *Secure erase* -- discard a range of data, ensuring that it can't be read back;
#. *ioctl* -- perform some arbitrary, driver-specific action.

There also a few request types dedicated to the management of `"zoned" devices <https://zonedstorage.io/introduction/zoned-storage/>`_, such as `shingled magnetic recording HDDs <https://zonedstorage.io/introduction/smr/>`_.
We do not discuss this feature here as BDUS does not currently support it.

.. rubric:: (Draft)

- Clarify relation between the three block sizes.

- Clarify atomicity guarantees (in both the context of concurrency and of crash safety) for writes, in particular of the physical block size.

.. .......................................................................... ..

Caches
------

.. figure:: /images/block-device-caches.*
    :align: right
    :width: 130px

    ..

We talked about how block devices can have caches, and about the flush and write
FUA requests. These caches can be, for example, some volatile memory inside the
HDD or SSD. However, this is not the only cache. The operating system also
provides a cache for the block device. When users read from the block device, if
the OS cache contains the requested data, it is immediately returned to the user
and the read request is *not* forwarded to the driver.

This cache is a *write-back* cache, meaning that write requests modify the cache
but may not be immediately propagated to the driver. This means that if the
system crashes, for instance, data in that cache but not persisted is lost. The
user of the block device can circumvent this by flushing that cache. It is also
possible for the user to completely bypass that cache (*e.g.*, by opening the
block device with the ``O_DIRECT`` flag to ``open()``).

So, to clarify, every block device always has one cache above. But the driver
doesn't have to care about it. Only if the actual underlying store used by the
driver has some kind of write-back caching. In that case, the driver should
properly handle FLUSH requests (and FUA write requests if possible, to enable
better performance).

One (and only one) page cache per device (major-minor pair). Can be bypassed by opening with O_DIRECT.

These two short articles are recent and a good summary of the Linux "block layer":

- https://lwn.net/Articles/736534/
- https://lwn.net/Articles/738449/

**Cool note:**
ioctl ``BLKFLSBUF`` only flushes OS cache, and also invalidates (clears) it; ``fsync()`` and ``fdatasync()`` are equivalent for block devices and they flush the OS cache and then flush the driver cache (by issuing a FLUSH request), and also do not invalidate the OS cache.

.. .......................................................................... ..

Partitions
----------

Document how block device partitions work.
How and why and when block devices are partitioned.
Document **when and under what circumstances** the kernel tries to recognize the partitions (tries to read the partition tables) on a device.

The single non-serious trouble that may arise from using an unpartitioned device directly: the kernel recognizing it as partitioned due to "random" data being interpreted as partition tables.
How this may be solved by partitioning as a single partition.
**Actually, this pretty much justifies the inclusion of an attribute to disable attribute scanning. Do that. Then document here that the attribute solve the aforementioned issue.**

.. .......................................................................... ..
