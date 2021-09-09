.. .......................................................................... ..

.. _open-sourcing:

(Open sourcing)
===============

#. The ``LIBBDUS_SONAME`` variable in the ``Makefile`` should be changed from ``libbdus.so.$(VERSION)`` to ``libbdus.so.$(VERSION_MAJOR).$(VERSION_MINOR)``.

#. Search the repository for all (case-insensitive) matches of ``gitlab`` and resolve them.

#. Remove this page.

#. Follow the procedure in specified in :ref:`creating-releases` to create the 0.1.0 release.

#. Setup *GitHub Actions* and *Read The Docs*.

#. Activate Google Analytics in the configuration of *Read The Docs*.

.. .......................................................................... ..
