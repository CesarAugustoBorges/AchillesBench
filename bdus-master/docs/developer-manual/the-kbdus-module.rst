.. .......................................................................... ..

.. _the-kbdus-module:

The *kbdus* module
==================

"contains the first description of the BDUS kernel module, and explains why and how several alternative implementations are provided"

.. .......................................................................... ..

Design
------

High-level architecture
~~~~~~~~~~~~~~~~~~~~~~~

TODO: Diagram.

Devices are identified by a uniqueid, unique since the module was loaded.

ioctl(STOP_BY_INDEX, &index);

index is set to uniqueid if ioctl fails with EAGAIN.

ioctl(STOP_BY_UNIQUEID, &index);

This allow restarting the ioctl without race conditions.

Global lock for device creation, destruction, and registry operations. Global completion triggered whenever device is destroyed. All stop ioctls check the registry to see if device with same uniqueid exists. If not, device was destroyed.

bdus-info: Gives info on a device. All info in human readable form if no options given. Otherwise, one numeric value per line for each option, in the requested order.

Communication protocols
~~~~~~~~~~~~~~~~~~~~~~~

**Why protocols?** Protocols currently allow us to experiment with, benchmark, and profile several techniques for implementing the communication between kernel- and user-space. They will probably also be included in "production-ready" releases, allowing the user to select a specific protocol if deemed necessary or advantageous (e.g., to bypass bugs or to select a protocol with better performance for a particular workload).

The kernel module supports several methods of communication from and to user-space for the implementation of device drivers. These are called "protocols".

Protocols are identified by a string. The protocol to be used is selected at module load-time. User-space must then use that protocol to communicate with the kernel module.

.. .......................................................................... ..

The *kbdus* module
------------------

- Some cool info on blk-mq:

  - http://ari-ava.blogspot.com/2014/06/opw-linux-block-io-layer-part-1-base.html
  - http://ari-ava.blogspot.com/2014/06/opw-linux-block-io-layer-part-2-request.html
  - http://ari-ava.blogspot.com/2014/07/opw-linux-block-io-layer-part-3-make.html
  - http://ari-ava.blogspot.com/2014/07/opw-linux-block-io-layer-part-4-multi.html

- kbdus' and libbdus' APIs are C99.

- Developer Manual: Document *kbdus*' user-space interface.

- kbdus: The seqnum exists so that user space can refer to some particular device without race conditions (seqnums are not reused unless module is reloaded, indices are reused). Note that libbdus does not currently expose this attribute.

::

    Device states:

        INITIALIZED   Device was created but is still not available to clients.
        AVAILABLE     Device is fully functional and available to clients.
        TERMINATED    Device was terminated and is not available to clients.

    Device state transitions:

        INITIALIZED --> AVAILABLE
        INITIALIZED --> TERMINATED
        AVAILABLE   --> TERMINATED

Base interface
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The "ioctl" protocol
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The "uds" protocol
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. .......................................................................... ..

User-space API reference
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. rubric:: Protocol-agnostic interface

.. doxygenstruct:: kbdus_version
    :project: kbdus
    :no-link:

.. doxygenstruct:: kbdus_config
    :project: kbdus
    :no-link:

.. doxygenstruct:: kbdus_device_config
    :project: kbdus
    :no-link:

.. doxygenstruct:: kbdus_protocol_config
    :project: kbdus
    :no-link:

.. doxygenenum:: kbdus_request_type
    :project: kbdus
    :no-link:

.. doxygendefine:: KBDUS_IOCTL_TYPE
    :project: kbdus
    :no-link:

.. doxygendefine:: KBDUS_IOCTL_GET_VERSION
    :project: kbdus
    :no-link:

.. doxygendefine:: KBDUS_IOCTL_CREATE_DEVICE
    :project: kbdus
    :no-link:

.. doxygendefine:: KBDUS_IOCTL_ATTACH_TO_DEVICE
    :project: kbdus
    :no-link:

.. doxygendefine:: KBDUS_IOCTL_DEVICE_INDEX_TO_SEQNUM
    :project: kbdus
    :no-link:

.. doxygendefine:: KBDUS_IOCTL_FLUSH_DEVICE
    :project: kbdus
    :no-link:

.. doxygendefine:: KBDUS_IOCTL_REQUEST_SESSION_TERMINATION
    :project: kbdus
    :no-link:

.. doxygendefine:: KBDUS_IOCTL_REQUEST_DEVICE_DESTRUCTION
    :project: kbdus
    :no-link:

.. doxygendefine:: KBDUS_IOCTL_WAIT_UNTIL_DEVICE_IS_DESTROYED
    :project: kbdus
    :no-link:

.. rubric:: Interface for protocol "ioctl"

.. doxygenstruct:: kbdusioctl_request
    :project: kbdus
    :no-link:

.. doxygenstruct:: kbdusioctl_reply
    :project: kbdus
    :no-link:

.. doxygenunion:: kbdusioctl_reply_and_request
    :project: kbdus
    :no-link:

.. doxygendefine:: KBDUSIOCTL_IOCTL_RECEIVE_REQUEST
    :project: kbdus
    :no-link:

.. doxygendefine:: KBDUSIOCTL_IOCTL_SEND_REPLY
    :project: kbdus
    :no-link:

.. doxygendefine:: KBDUSIOCTL_IOCTL_SEND_REPLY_AND_RECEIVE_REQUEST
    :project: kbdus
    :no-link:

.. rubric:: Interface for protocol "mmap"

.. doxygenstruct:: kbdusmmap_request
    :project: kbdus
    :no-link:

.. doxygenstruct:: kbdusmmap_reply
    :project: kbdus
    :no-link:

.. doxygenunion:: kbdusmmap_reply_and_request
    :project: kbdus
    :no-link:

.. doxygendefine:: KBDUSMMAP_IOCTL_RECEIVE_REQUEST
    :project: kbdus
    :no-link:

.. doxygendefine:: KBDUSMMAP_IOCTL_SEND_REPLY
    :project: kbdus
    :no-link:

.. doxygendefine:: KBDUSMMAP_IOCTL_SEND_REPLY_AND_RECEIVE_REQUEST
    :project: kbdus
    :no-link:

.. .......................................................................... ..

``inverter.h/.c``
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Put a diagram of the request state FSM here.

*inverter*: Converts the push-based architecture of deice into a pull-based interface. Provides to device an interface to push requests and to protocol an interface to pull those requests.

.. .......................................................................... ..

Old notes
~~~~~~~~~~~~~~~~~~~~~~~~~~~

``CREATE_DEVICE``: This ioctl only starts the device creation process. Upon
returning, the device is still not fully created. This is because, for finishing
the creation process, the driver has first to reply to several requests. (In
more detail, these requests are related to partition tables and stuff, and are
issued by the add_disk() kernel function. That function only returns after the
requests are satisfied, and the block device file in /dev is only guaranteed to
exist after that function returns.)

Block devices are created by first opening the /dev/bdus-control device and then
issuing a specific ioctl call on the respective file descriptor. Whoever has
access to the file description referred to by the aforementioned file descriptor
can control the newly created block device.

After the file description in question is released, operations on the block
device will always fail with error EIO. The block device will then ultimately be
destroyed and removed from /dev when no file descriptions refer to it.

.. rubric:: ``add_disk``

Note: add_disk requires that requests be satisfied before returning (due to
partition table scanning), so the device must be able to satisfy requests before
invoking add_disk.

The block device driver must be fully able to respond to requests before calling
add_disk(), which is usually the last call required to make a block device
available.

This means that kernel-user communication must be fully established even before
the block device is fully created.

.. .......................................................................... ..
