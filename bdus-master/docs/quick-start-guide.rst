.. .......................................................................... ..

.. _quick-start-guide:

Quick Start Guide
=================

This guide describes the basics of block devices and provides an overview of BDUS' features by demonstrating the implementation a simple RAM disk.

Other parts of the documentation:

* The :ref:`user-manual`, describing all of the framework's functionalities;
* The :ref:`developer-manual`, for contributors and those curious about BDUS' internals.

.. .......................................................................... ..

Block devices
-------------

*TODO: The Quick Start Guide's section on block devices should probably be something more along the lines of the motivation in the OTPOUSBD paper, with less focus on hardware and more on the interface itself, its modern uses, how it is pervasive to essentially all systems that use storage, and how it is thus a great abstraction level in which to add storage functionality in a transparent manner.*

Let us start with a few paragraphs on Linux block devices.

*Block devices* provide access to devices that transfer randomly accessible data in fixed-size blocks [CoRK05]_, such as hard disk drives (HDD) and solid-state drives (SSD), presenting data as a linear sequence of bytes.
They hide the intricacies of specific hardware devices by exposing a uniform interface, while also contributing to their efficient use (*e.g.*, by reordering requests and providing a write-back cache).

Block devices are made available to user-space applications through `block special files <https://en.wikipedia.org/wiki/Device_file#Block_devices>`_, usually created under the ``/dev`` `directory <https://en.wikipedia.org/wiki/Udev>`_.
This mechanism makes it possible to access block devices as if they were regular files (*e.g.*, by using standard system call wrappers like ``open``, ``read``, and ``write``).

To make a hardware device available to the system through the uniform block device interface, one must implement a *block device driver*.
This implies having to write kernel code (possibly in the form of a `kernel module <https://en.wikipedia.org/wiki/Loadable_kernel_module#Linux>`_) that specifies how operations on the block device should be translated to hardware-specific commands.

Writing in-kernel block device drivers is a non-trivial endeavor.
This is where the BDUS framework comes in: BDUS allows you to write block device drivers in user space, saving you the hassle of kernel programming and debugging in the cases where the driver can be implemented as a user-level application.

It is worth noting that not all block devices map directly to hardware storage devices.
Examples of such block devices are `NBD <https://en.wikipedia.org/wiki/Network_block_device>`_ (which redirects requests to a remote machine over a TCP connection) and the RAM disk whose driver is implemented in this guide.

.. .......................................................................... ..

Installing BDUS
---------------

If you haven't already, you need to install BDUS.
To do this, download BDUS' `latest stable release <release-tar_>`_, extract it, and then use the provided Makefile by running the following command:

.. code-block:: console

    $ sudo make install

To revert all changes made by the installation, you can run the following command:

.. code-block:: console

    $ sudo make uninstall

.. .......................................................................... ..

Writing a driver
----------------

In order to demonstrate the basics of using BDUS to implement block device drivers, we are going to develop a simple user-space RAM disk.
This kind of block device stores its contents in the system's RAM.

We must first include the ``bdus.h`` header file to gain access to the BDUS user-space API, and also some standard C headers that we are going to need:

.. code-block:: c

    #include <bdus.h>

    #include <stdbool.h>
    #include <stdint.h>
    #include <stdlib.h>
    #include <string.h>

To specify the behavior of our device, we have to implement its *operations*.
These are simple C functions that are invoked by the BDUS framework whenever a request is received.

There are several types of operations that a driver can implement.
For our simple RAM disk, implementing the ``read`` and ``write`` operations suffices:

.. code-block:: c

    static int device_read(
        char *buffer, uint64_t offset, uint32_t size,
        struct bdus_dev *dev
        )
    {
        memcpy(buffer, (char *)dev->user_data + offset, size);
        return 0;
    }

    static int device_write(
        const char *buffer, uint64_t offset, uint32_t size,
        struct bdus_dev *dev
        )
    {
        memcpy((char *)dev->user_data + offset, buffer, size);
        return 0;
    }

The ``device_read`` function will be called whenever a *read* request is submitted to our device, while ``device_write`` will be invoked whenever a *write* request is received.
Their parameters have the following meanings:

* ``buffer``: a buffer in which the requested data should be placed, or whose contents should be written to the device;
* ``offset``: the offset (in bytes) into the device at which the read or write should take place;
* ``size``: the number of bytes that should be read or written;
* ``dev``: information about the device that received the request.

For our RAM disk to function, we would first need to allocate a memory buffer where the device's contents would be stored.
For now, we assume that a pointer to such a buffer is available in these callbacks as ``dev->user_data``.
Since this pointer has type ``void *``, we cast it to ``char *`` to be able to add the offset to it.

The implementations of the two functions above are straightforward: they simply copy data between the request buffer and our RAM buffer.
As these operations can never fail, we ``return 0`` to indicate that all went fine.

All callbacks for a device must be stored in a value of type :type:`struct bdus_ops <bdus_ops>`, which will later be passed on to BDUS:

.. code-block:: c

    static const struct bdus_ops device_ops =
    {
        .read  = device_read,
        .write = device_write,
    };

Using this type of initialization, the remaining fields of the structure are set to ``NULL``, which will inform BDUS that our driver does not support other types of requests.

Now that we have implemented our device's operations, we have to configure some of its *attributes*.
Among these are the device's *size* --- the number of bytes that the device can store --- and *logical block size* --- the smallest size that the driver is able to address.
Requests submitted to our driver will always be aligned to the logical block size.

.. *physical block size* -- the smallest size that the driver can operate on without reverting to read-modify-write operations.

.. Users of the device will also try to align their requests to the physical block size in order to avoid the performance penalty of read-modify-write operations.

.. Setting these attributes to the appropriate values is of utmost importance.

We'll set our device's size to the arbitrary value of 1 GiB.
For our simple driver, setting its logical block size to the minimum allowed value of 512 bytes is adequate.

To later pass these attributes to BDUS, we have to store them in a value of type :type:`struct bdus_attrs <bdus_attrs>`:

.. code-block:: c

    static const struct bdus_attrs device_attrs =
    {
        .size               = 1 << 30, // 1 GiB
        .logical_block_size = 512,
    };

By using this type of initialization, we ensure that the remaining fields of the structure are set to zero, which will later instruct BDUS to pick default values for them.
The two attributes that we have set are the only mandatory ones.

Note that several restrictions are imposed on the values of these attributes; for more details, check the documentation for :type:`struct bdus_attrs <bdus_attrs>`.

Finally, and because our driver is meant to be compiled as an executable program, we have to implement the ``main()`` function:

.. code-block:: c

    int main(void)
    {
        void *buffer = malloc(device_attrs.size);

        if (!buffer)
            return 1;

        bool success = bdus_run(&device_ops, &device_attrs, buffer);

        free(buffer);

        return success ? 0 : 1;
    }

Here, we first allocate the RAM buffer that will be used to store our device's contents, and then run the driver by calling :func:`bdus_run`.
This function receives the following arguments:

#. A pointer to the structure containing the device's attributes;
#. A pointer to the structure containing the device's operations;
#. The initial value for the ``dev->user_data`` field that is available from the device's callbacks (this is why we used it as a pointer to our RAM buffer).

That is it.
By concatenating all the previous code excerpts, you should get a fully functional RAM disk driver.
The full source code for this driver is available in the repository at :repo-file:`examples/ram-simple.c`.
Other example drivers can also be found in the repository's :repo-dir:`examples/` directory.

.. .......................................................................... ..

Compiling and using the driver
------------------------------

Now that our driver is implemented, we must compile it as an executable application.

Let's say that the driver's source code is entirely contained in file ``driver.c``.
In this case, you can compile it by typing the following command:

.. code-block:: console

    $ cc driver.c -lbdus -o driver

The ``-lbdus`` flag tells the linker to link our program against BDUS' driver development library.
Assuming that the program was compiled successfully, there should now be an executable file named ``driver`` in the current directory.
To create a block device powered by our driver, simply run that executable as the superuser:

.. code-block:: console

    $ sudo ./driver

If everything goes fine, the path to our RAM disk's block device will be printed and you will regain control of the terminal.
From this point on, that block device is available for anyone (with sufficient privileges) to use.

Let's assume that everything did go alright, and that the driver printed ``/dev/bdus-0``.
We may now, for example, create a file system on that device:

.. code-block:: console

    $ sudo mkfs.ext4 /dev/bdus-0

This command will effectively create an `ext4 <https://en.wikipedia.org/wiki/Ext4>`_ file system that resides in the system's RAM.
We can then `mount <https://en.wikipedia.org/wiki/Mount_(computing)>`_ and use that file system:

.. code-block:: console

    $ mkdir mount-point
    $ sudo mount /dev/bdus-0 mount-point

To unmount the file system, use the following command:

.. code-block:: console

    $ sudo umount mount-point

After you finish playing with your new RAM disk, you may want to remove it from your system.
To do so, run the following command:

.. code-block:: console

    $ sudo bdus destroy /dev/bdus-0

This command returns after the device is destroyed.
Destroy a BDUS device by using this command will safely persist all previously written data, end execution of its driver, and remove it from ``/dev``.

.. .......................................................................... ..

What's next?
------------

You may now want to read the :ref:`user-manual`, which provides an in-depth description of the framework's capabilities and API for driver development.
Its :ref:`api-reference` section is a particularly handy resource for those developing drivers with BDUS.

For detailed information on in-kernel block device drivers (and Linux drivers in general), see [CoRK05]_.

.. .......................................................................... ..

.. raw:: html

    <hr class="docutils" style="margin-top: 30px;">

.. rubric:: References

.. [CoRK05] |nbsp| J. Corbet, A. Rubini, G. Kroah-Hartman, `Linux Device Drivers <https://lwn.net/Kernel/LDD3/>`_, 3rd edition

.. .......................................................................... ..
