.. .......................................................................... ..

.. _installation:

Installation
============

.. raw:: html

    <strike>

Releases can be downloaded from the GitHub repository's `Releases <https://github.com/albertofaria/bdus/releases>`_ section.
BDUS is compatible with Linux 4.0 and above.

.. raw:: html

    </strike>

For now, releases are available `here <https://gitlab.lsd.di.uminho.pt/albertofaria/bdus/-/tags>`_.

.. .......................................................................... ..

Basic instructions
------------------

.. rubric:: Installing

BDUS can be installed using the Makefile included with every release.
To install BDUS, download and extract the desired release and run the following command:

.. code-block:: console

    $ sudo make install

This will install the *kbdus* kernel module, the *libbdus* driver development library, and the ``bdus`` command.
Already installed *libbdus* versions are not removed, allowing drivers compiled against other *libbdus* versions to continue to function (if those versions are compatible with the installed kernel module).

Note that if you later switch to a different Linux kernel version, a reinstallation might be necessary.
In that case, simply rerun the above command.

.. rubric:: Uninstalling

The same Makefile can be used to uninstall BDUS.
Run the following command to revert all changes made by the installation:

.. code-block:: console

    $ sudo make uninstall

.. .......................................................................... ..

Advanced options
----------------

While the ``install`` Makefile target installs all three aforementioned components, the ``install-kbdus``, ``install-libbdus``, and ``install-bdus`` targets can be used to install each component separately.

The ``install`` and ``install-libbdus`` targets install the *libbdus* shared library, the ``bdus.h`` header file that exposes *libbdus*' API, and makes it so the ``-lbdus`` linker flag refers to the installed shared library.
This is a so-called "development installation" of *libbdus*, as it allows users to compile drivers against it.

However, one may wish to install a previous version of *libbdus* without replacing a currently installed, more recent version.
This might be necessary to use drivers compiled against an older *libbdus* version.
For this purpose, one can use the ``install-libbdus-nodev`` target, which only installs the shared library, without replacing the ``bdus.h`` header and without changing the ``-lbdus`` linker flag to point to the older *libbdus* version.

Note that, unlike *libbdus*, only one version of *kbdus* and command ``bdus`` can be installed at any time.

Finally, one can customize the directories to which BDUS is installed by setting the ``KBDUS_INSTALL_DIR``, ``LIBBDUS_HEADER_INSTALL_DIR``, ``LIBBDUS_BINARY_INSTALL_DIR``, ``CMDBDUS_BINARY_INSTALL_DIR``, and ``CMDBDUS_COMPLETION_INSTALL_DIR`` environment variables.
Check the :repo-file:`Makefile` to see how these are used.

.. .......................................................................... ..
