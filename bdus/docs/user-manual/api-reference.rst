.. .......................................................................... ..

.. _api-reference:

API reference
=============

This page contains reference documentation for the entire C99 API of *libbdus*, BDUS' user-space driver development library, which is exported by the ``bdus.h`` header file.

For a more introductory, running description of the driver development interface and features provided by BDUS, see :ref:`developing-drivers`.

.. TODO: uncomment the following rubric when moving to 1.0.0

.. .. rubric:: A note on backward compatibility

.. API preconditions may be relaxed in future releases (*e.g.*, parameter values that currently cause a function to fail or exhibit undefined behavior may not do so in the future), even if they have the same *major* version.
.. ``errno`` values set on function failure and error messages returned by :func:`bdus_get_error_message` may also change.
.. See :ref:`versioning-scheme-and-backward-compatibility` for more details.

.. rubric:: Summary

* :ref:`driver-development`:
    * :func:`bdus_run`
    * :func:`bdus_rerun`
    * :type:`struct bdus_dev <bdus_dev>`
    * :type:`struct bdus_ops <bdus_ops>`
    * :type:`struct bdus_attrs <bdus_attrs>`

* :ref:`device-management`:
    * :func:`bdus_flush_dev`
    * :func:`bdus_destroy_dev`

* :ref:`device-indices-and-paths`:
    * :func:`bdus_dev_index_to_path`
    * :func:`bdus_dev_path_to_index`
    * :func:`bdus_dev_index_or_path_to_index`

* :ref:`errors`:
    * :func:`bdus_get_error_message`

* :ref:`version`:
    * :type:`struct bdus_version <bdus_version>`
    * :func:`bdus_get_libbdus_version`
    * :func:`bdus_get_kbdus_version`

.. .......................................................................... ..

.. _driver-development:

Driver development
------------------

.. doxygenfunction:: bdus_run
.. doxygenfunction:: bdus_rerun
.. doxygenstruct:: bdus_dev
.. doxygenstruct:: bdus_ops
.. doxygenstruct:: bdus_attrs

.. .......................................................................... ..

.. _device-management:

Device management
-----------------

.. doxygenfunction:: bdus_flush_dev
.. doxygenfunction:: bdus_destroy_dev

.. .......................................................................... ..

.. _device-indices-and-paths:

Device indices and paths
------------------------

.. doxygenfunction:: bdus_dev_index_to_path
.. doxygenfunction:: bdus_dev_path_to_index
.. doxygenfunction:: bdus_dev_index_or_path_to_index

.. .......................................................................... ..

.. _errors:

Errors
------

.. doxygenfunction:: bdus_get_error_message

.. .......................................................................... ..

.. _version:

Versions
--------

.. doxygenstruct:: bdus_version
.. doxygenfunction:: bdus_get_libbdus_version
.. doxygenfunction:: bdus_get_kbdus_version

.. .......................................................................... ..
