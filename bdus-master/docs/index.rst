.. .......................................................................... ..

BDUS: Block Devices in User Space
=================================

|build-badge| |version-badge| |license-badge|

BDUS is a Linux 4.0+ framework for developing **block devices in user space**.
More specifically, it allows you to implement *block device drivers* as regular user-space applications written in C.

This is the entry page for BDUS' documentation, which is divided into three parts:

* The :ref:`quick-start-guide`, serving as an introduction to the framework;
* The :ref:`user-manual`, describing all of the framework's functionalities;
* The :ref:`developer-manual`, for contributors and those curious about BDUS' internals.

To get started, take a look at the :ref:`quick-start-guide`.
See also the :ref:`api-reference`.

.. warning::

    BDUS is not yet mature enough for production use, and bugs in its kernel component can cause system instability or even data corruption. Please avoid using BDUS on your personal computer or any other machine running critical services or storing important data.

.. raw:: html

    <strike>

BDUS is maintained on `GitHub <https://github.com/albertofaria/bdus>`_.

.. raw:: html

    </strike>

BDUS is currently available on our `private GitLab instance <https://gitlab.lsd.di.uminho.pt/albertofaria/bdus>`_.

**Installation**
    Download BDUS' latest release `here <release-tar_>`_.
    To install it, run ``sudo make install``.
    For more details, see :ref:`installation`.

**Licensing**
    BDUS is distributed under the terms of the :repo-file:`MIT license <LICENSE-MIT.txt>`, with the exception of its kernel module which is distributed under the terms of the :repo-file:`GPLv2 license <LICENSE-GPLv2.txt>`.
    For more details, see :ref:`licensing`.

**Contributing**
    .. raw:: html

        <strike>

    To report bugs, suggest improvements, or propose new features, please use GitHub's `issue tracking system <https://github.com/albertofaria/bdus/issues>`_.
    For information on how to contribute changes, see :ref:`contributing`.

    .. raw:: html

        </strike>

    For now, please DM me (\@alberto.c.faria) on the ds\@haslab Mattermost server or email me at alberto.c.faria@inesctec.pt.

.. .......................................................................... ..

.. toctree::
    :hidden:

    self

.. toctree::
    :hidden:
    :includehidden:
    :maxdepth: 3
    :numbered: 2

    quick-start-guide
    user-manual/index
    developer-manual/index

.. .......................................................................... ..
