.. .......................................................................... ..

.. _architecture:

Architecture
============

The kbdus module and the libbdus library are further detailed in sections :ref:`the-kbdus-module` and :ref:`the-libbdus-library`.
The ``bdus`` command currently only provides a ``destroy`` subcommand, and is just a dumb wrapper over libbdus' :func:`bdus_destroy_dev` function.

.. rubric:: Interface

- ``/dev/bdus-control``, a character device created right when the module is
  inserted.

.. rubric:: Architecture

Components: *registry*, *control*, *devices*, *protocols*.

- All 3 components are packaged and versioned together as a single system.

.. rubric:: Why a single project with two sub-projects?

BDUS is composed of two sub-projects: (1) a *kernel module* (kbdus) and (2) a *user-space library* (libbdus). Why, then, are they maintained in the same repository?

kbdus and libbdus may be considered to be two very different projects. kbdus is licensed under the GPLv2 license, while libbdus is licensed under the MIT license. kbdus would ideally be versioned differently from libbdus.

However, distributing kbdus by itself would be fairly useless: its API is inherently difficult to use. Distributing libbdus by itself would also, of course, be useless: it depends on kbdus.

Having the two sub-projects in the same repository means that kbdus and libbdus are developed in tandem. As such, development of libbdus never falls behind that of kbdus. It also means that installation is trivial, as both sub-projects are installed simultaneously and are guaranteed to be compatible.

The disadvantages are that it complicates licensing and versioning. In particular, both sub-projects are forced to have the same version. Due to this, the versioning scheme for kbdus is somewhat awkward. We nevertheless try to adhere to `SemVer <https://semver.org/>`_. The main side-effect is that the version of kbdus or libbdus may be bumped without any change to that sub-project at all.

.. .......................................................................... ..
