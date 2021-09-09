.. .......................................................................... ..

.. _versions:

Versions
========

This section describes BDUS' versioning scheme and the backward compatibility guarantees provided across releases.
A list of all published versions and respective notable changes is also included.

.. .......................................................................... ..

.. _versioning-scheme-and-backward-compatibility:

Versioning scheme and backward compatibility
--------------------------------------------

.. warning::

    The following text will only apply once BDUS becomes open source.
    For now, version numbers are of the form *0.0.n* and **absolutely no API or ABI backward compatibility guarantees are provided** between releases.

This project adheres to the `SemVer 2.0.0 <https://semver.org/spec/v2.0.0.html>`_ versioning scheme.
Releases are identified by a *version number* of the form **x.y.z**, where **x**, **y**, and **z** are the *major*, *minor*, and *patch* versions, respectively.

These version numbers encode *public API* backward compatibility guarantees across releases.
The user-space driver development API as documented in the :ref:`api-reference`, together with the interface provided by the ``bdus`` command, constitute BDUS' public API.

(The user-space interface exposed by the *kbdus* kernel module is also part of the public API.
This is most relevant to those creating bindings for other languages.
See :ref:`the-kbdus-module` for more details.)

BDUS is currently in the "initial development" phase, meaning that every release has *major* version 0 and *no API or ABI backward compatibility guarantees are provided between releases with distinct* minor *versions*.
However, releases which differ only in the *patch* version are API and ABI compatible.

Specifically, releases that modify or extend any public interface trigger a *minor* version increment and reset the *patch* version to 0 (*e.g.*, 0.5.3 → 0.6.0).
Releases that do not modify any public interface (*i.e.*, bugfixes or implementation improvement releases) only trigger a *patch* version increment (*e.g.*, 0.6.0 → 0.6.1).

MORE STUFF: Note that drivers linked against a given *libbdus* version will simply fail to load if a compatible version is not installed.

MORE STUFF: libbdus 0.X._ works with kbdus 0.X._, refuses to do stuff otherwise.

.. note:: *The plan for 1.0.0 and above:*

    Releases with different major versions are not API or ABI compatible.
    Releases with same major and minor versions are API *and* ABI compatible.
    A release with same major number to another but greater minor number is API backward compatible with the release with the lower minor number, but *not necessarily* ABI compatible.
    (As a special case, kbdus' user-space interface also maintains ABI backward compatibility between releases with same major number.)

    Specifically, trigger major version increment if breaking API compatibility.
    Otherwise, trigger minor version increment if extending API in backward compatible manner or if breaking ABI compatibility.
    Otherwise, trigger patch version increment (no API or ABI changes).
    (May also choose to increment minor version even if no API or ABI changes if major implementation changes.)

    MORE STUFF: libbdus X.A._ works with kbdus X.B._ for A >= B, refuses to do stuff otherwise.

.. .......................................................................... ..

.. _version-history:

Version history
---------------

The following is a list of all releases that existed at the time BDUS |version| was released, in reverse version order.
All notable changes between releases are also documented here.

0.0.9 (2020-08-03)
~~~~~~~~~~~~~~~~~~

The following is a list of notable changes relative to version 0.0.8.
See also the :diff:`git diff <0.0.8...0.0.9>`.

- Add support for Linux 5.8.
- Remove command ``bdus flush``, as it is equivalent to ``sync``.
- Rename "secure discard" requests to "secure erase" to match Linux terminology.
- Add parameter ``may_unmap`` to callback ``write_zeros()`` matching kernel flag ``REQ_NOUNMAP``.

0.0.8 (2020-06-03)
~~~~~~~~~~~~~~~~~~

The following is a list of notable changes relative to version 0.0.7.
See also the :diff:`git diff <0.0.7...0.0.8>`.

- Mostly compatibility with Linux 5.7.

0.0.7 (2020-03-07)
~~~~~~~~~~~~~~~~~~

The following is a list of notable changes relative to version 0.0.6.
See also the :diff:`git diff <0.0.6...0.0.7>`.

- Significant documentation improvements.
- Allow simultaneous non-development installations of different *libbdus* versions.
- Allow interoperability between different versions of *kbdus*, *libbdus*, and command ``bdus``.
- Added protocol and backend "rw".

0.0.6 (2020-02-17)
~~~~~~~~~~~~~~~~~~

The following is a list of notable changes relative to version 0.0.5.
See also the :diff:`git diff <0.0.5...0.0.6>`.

- Added protocol "ioctl-mmap".
- Added support for driver replacement and recovery.
- Significant interface and implementation improvements.

0.0.5 (2019-12-20)
~~~~~~~~~~~~~~~~~~

The following is a list of notable changes relative to version 0.0.4.
See also the :diff:`git diff <0.0.4...0.0.5>`.

- Fixed handling of requests submitted after termination.

0.0.4 (2019-11-30)
~~~~~~~~~~~~~~~~~~

The following is a list of notable changes relative to version 0.0.3.
See also the :diff:`git diff <0.0.3...0.0.4>`.

- Device flushing now fully works.
- Makefile install targets now set file permissions.
- Devices are now reported as read-only when no write or discard operations are
  supported.
- *FUA write* requests can now be supported even if *write* request aren't.
- *libbdus* logging improvements.
- Code quality improvements.
- Documentation improvements.

0.0.3 (2019-11-14)
~~~~~~~~~~~~~~~~~~

The following is a list of notable changes relative to version 0.0.2.
See also the :diff:`git diff <0.0.2...0.0.3>`.

- Mostly bug fixes and code quality improvements.

0.0.2 (2019-11-09)
~~~~~~~~~~~~~~~~~~

The following is a list of notable changes relative to version 0.0.1.
See also the :diff:`git diff <0.0.1...0.0.2>`.

- Implementation now uses blk-mq.
- Many other things that I'm too lazy to write about.

0.0.1 (2019-09-05)
~~~~~~~~~~~~~~~~~~

First (arguably) working release.

.. .......................................................................... ..
