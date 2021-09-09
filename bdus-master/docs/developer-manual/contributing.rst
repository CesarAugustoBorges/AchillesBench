.. .......................................................................... ..

.. _contributing:

Contributing
============

This section describes the appropriate procedures for :ref:`reporting bugs <reporting-bugs>`, :ref:`suggesting improvements and features <suggesting-improvements-and-features>`, and :ref:`contributing changes <contributing-changes>`.
It also specifies how maintainers should :ref:`integrate changes <integrating-changes>` and :ref:`create releases <creating-releases>`.

We thank you in advance for your contributions to this project. :)

.. .......................................................................... ..

.. _reporting-bugs:

Reporting bugs
--------------

If you believe that you found a bug in the BDUS framework, please report it by creating an issue on GitHub's `issue tracking system <https://github.com/albertofaria/bdus/issues>`_.
Try to tell us how to reproduce the bug, and make sure to mention the version of BDUS that you are using.

.. .......................................................................... ..

.. _suggesting-improvements-and-features:

Suggesting improvements and features
------------------------------------

If you would like to see an improvement or feature being added to the BDUS framework, please let us know by creating an issue on GitHub's `issue tracking system <https://github.com/albertofaria/bdus/issues>`_.

.. .......................................................................... ..

.. _contributing-changes:

Contributing changes
--------------------

Anyone can propose changes to BDUS by creating `pull requests on GitHub <https://github.com/albertofaria/bdus/pulls>`_.
In general, please try to follow these rules:

- Non-bugfix pull requests should be targeted at branch *master*.

- Bugfix pull requests should be targeted at the branch corresponding to the most recent version to which the fix applies.

- If a bugfix also applies to the development version, or only to that version, the pull request would be targeted at branch *master*.

- The versions to which a bugfix can be applied should be clearly stated in the pull request description.

- If the same bug is also present in other versions, but cannot be simply fixed with the same or a very similar bugfix), create more pull requests.

**The change log.**
If you are contributing non-trivial changes, remember to update the *[Unreleased]* section of the ``CHANGELOG.md`` file accordingly.
Note that the change log for a particular branch must only contain changes for versions reachable from that branch.
For instance, branch *master*'s change log should never contain any entries for versions 1.3.1 or 2.7.13 (or any other version with a non-zero patch version).

.. .......................................................................... ..

.. _branching-model:

Branching model
---------------

Here we describe the branching model used in BDUS' repository.
See also the :ref:`versioning-scheme-and-backward-compatibility` section.

.. rubric:: Branching and tagging

.. figure:: /images/branching-model.*
    :align: right
    :width: 170px

    ..

Branch **master** corresponds to the latest usable, development version of the project, and must always pass all tests.
Other branches may be created to work on new features or fixes, and all modifications must be done in such a branch and pass all tests before being integrated into **master**.
These are called *feature* branches and may be deleted after being merged into **master** or if given up on.

Releases with patch version 0 correspond to commits in branch **master** and are marked by tags.
The name of the tag is the corresponding release's version number (e.g., **1.0.0**).

If a release with non-zero patch version is created, say **x.y.z**, it must correspond to a commit in a branch named **x.y** that diverges from **master** at the commit corresponding to the **x.y.0** release, and must also be tagged with the release's version number.
These so called *release* branches must only be created when a **x.y.1** release is created.

This branching model is depicted in the figure to the right, where tags are displayed in blue and branches in red.
Note that this model applies to both the initial development and production stages.

No tags should be deleted.
No *release* branches should be deleted.
GitHub releases should not be created (mostly because the "latest release" label is applied to the most recently created release, not to the one with the highest version).
"Release candidates" and the like are not allowed.

.. rubric:: Integrating changes

Merge commits are not allowed, as they can make the history very complicated very quickly.
Only fast-forwarding
*Feature* branches must be integrated into master only through fast-forwarding.
Rebase *feature* branches when necessary, and consider squashing commits or otherwise reorganizing them in order to simplify history.

Merging GitHub pull requests must follow the same rules (the changes are being merged from some other branch).

Note that these rules guarantee the resulting history to be a tree.

The complete procedure for integrating changes is described in sub-section :ref:`integrating-changes`.

.. rubric:: Limitations

This branching model does not allow for the creation of new releases with major numbers lower than that of the latest release.

.. .......................................................................... ..

For maintainers
---------------

.. _integrating-changes:

Integrating changes
~~~~~~~~~~~~~~~~~~~

Maintainers are people with write access to the GitHub repository.
If you are a maintainer, this subsection is for you.

**Create pull requests just like non-maintainer contributors.**
Maintainers are encouraged to follow the same contribution procedure as non-maintainer contributors, *i.e.*, creating pull requests.
However, feel free to create "feature" branches in the official repository and pull requests from those branches to **master**.

**Integrating pull requests.**
Avoid creating merge commits (they can make the repository history very complicated very fast).
Instead, integrate pull requests by rebasing and fast-forwarding.
Also, if a pull request is a bugfix for more than one version, apply the pull request to the respective target branch and then cherry-pick its commits to all the remaining branches to which the bugfix applies.

.. _creating-releases:

Creating releases
~~~~~~~~~~~~~~~~~

Maintainers should follow this procedure when creating new releases:

#. Do a global find for all occurrences of ``TODO``, ensuring that no TODO items apply to the new release.

#. Add an entry for the new release in the :ref:`version-history` with all relevant changes since the release from which the new one descends. Note that the version history in branch **master** should also be updated to contain releases with non-zero patch versions.

#. Update occurrences of the current version:

  - ``Makefile``: Update the ``VERSION`` variable.
  - ``README.md``: Update the download link for the latest stable release.
  - ``README.md``: Update the version badge.

#. Do a global find for all occurrences of the current version in the whole repository. Update them where applicable and add them to this procedure if they are not enumerated in the list above.

#. Create commit with these changes.

#. Create tag on this new commit whose name is simply the new version (*without* any prefix such as ``v``).

#. Ensure that the *Read the Docs* documentation build succeeded. Ensure that the new version is "active" (*i.e.*, shows up in the version selection menu). Remove old versions of the documentation (by making them inactive) if appropriate.

#. Ensure that *Github Actions* passed.

.. .......................................................................... ..
