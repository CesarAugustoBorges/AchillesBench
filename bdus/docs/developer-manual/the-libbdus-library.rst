.. .......................................................................... ..

.. _the-libbdus-library:

The *libbdus* library
=====================

- http://tldp.org/HOWTO/Program-Library-HOWTO/shared-libraries.html has some info on sonames.

- kbdus' and libbdus' APIs are C99.

- libbdus' soname is libbdus.X.Y for BDUS version X.Y._.

.. rubric:: Automatically loading *kbdus*

Ideal would be to use libkmod, but its headers are frequently not installed by default, and as such it would become an explicit dependency and complicate the installation process (BDUS currently has no explicit dependencies).

Another option would be to add kbdus to the list of modules to be loaded at boot, but the location and format of this list varies across Linux distributions.

So we settled with an exec of /sbin/modprobe, which should work everywhere (/sbin is a standardized Linux directory, and modprobe is surely always available).

.. rubric:: Extra configuration options

libbdus also exposes variants of :func:`bdus_run` and :func:`bdus_rerun` that allow you to specify a different "internal configuration":

.. doxygenfunction:: bdus_run_with_internal_config_
    :no-link:

.. doxygenfunction:: bdus_rerun_with_internal_config_
    :no-link:

These are just like :func:`bdus_run` and :func:`bdus_rerun`, but receive an extra parameter pointing to a ``struct bdus_internal_config_``:

.. doxygenstruct:: bdus_internal_config_
    :no-link:

Note that *none* of these functions and structs are part of BDUS' public API (notice that they end with an underscore).

.. .......................................................................... ..

Other things
------------

How are devices destroyed?

  - If the driver dies, it closes its control FD which causes the kernel-side
    device and state for the device to be destroyed.

  - If device termination is requested, the kernel informs the driver of that
    request and the driver then closes its control FD, resulting in the same
    as above.

  - Note that device termination requests might only be sent to the user-space
    driver after pending requests, or requests to write previously written but
    cached or non-persisted data, are serviced by the user-space driver. This
    depends on the kind of termination request that was submitted: "safe" or
    "immediate".

.. .......................................................................... ..
