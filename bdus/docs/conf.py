# ---------------------------------------------------------------------------- #

# Configuration file for the Sphinx documentation builder.

# ---------------------------------------------------------------------------- #
# imports

import datetime
import re

# ---------------------------------------------------------------------------- #
# project information

project   = "BDUS"
author    = "Alberto Faria"
copyright = "{year}, {author}".format(
    year=datetime.datetime.now().year,
    author=author
    )

with open("../Makefile", "r", encoding="utf-8") as f:
    [v] = re.findall(r"(?m)^VERSION := (\d+\.\d+\.\d+)$", f.read())

version = release = v

# ---------------------------------------------------------------------------- #
# general configuration

needs_sphinx = "2.4"

extensions = [
    "sphinx.ext.extlinks",
    "breathe",
    "versionwarning.extension"
    ]

exclude_patterns = ["_build", "Thumbs.db", ".DS_Store"]

primary_domain = "c"
highlight_language = "none"
pygments_style = "sphinx"

rst_prolog = """

.. |nbsp| unicode:: 0xA0

.. _release-tar: https://gitlab.lsd.di.uminho.pt/albertofaria/bdus/-/archive/{version}/bdus-{version}.tar.gz

.. |build-badge| image:: https://github.com/albertofaria/bdus/workflows/build/badge.svg?branch=master
    :target: https://github.com/albertofaria/bdus/actions

.. |version-badge| image:: https://img.shields.io/badge/version-{version}-yellow.svg
    :target: https://gitlab.lsd.di.uminho.pt/albertofaria/bdus/-/tags

.. |license-badge| image:: https://img.shields.io/badge/license-MIT%20%2F%20GPLv2-blue.svg
    :target: user-manual/licensing.html

""".format(version=version)

# TODO: remove when repo is public
linkcheck_ignore = [
    r"https://github\.com.*",
    r"https://gitlab\.lsd\.di\.uminho\.pt.*",
]

# ---------------------------------------------------------------------------- #
# html output configuration

html_copy_source       = False
html_use_index         = False
html_scaled_image_link = False

html_theme = "alabaster"

html_theme_options = {
    "logo": "logo.svg",
    "github_user": "albertofaria",
    "github_repo": "bdus",
    "github_button": True,
    "github_type": "star",
    "github_count": "true",
    "page_width": "1000px",
    "sidebar_width": "220px",
    "show_relbar_bottom": True,
    "fixed_sidebar": True,
}

html_sidebars = { "**": ["about.html", "navigation.html", "searchbox.html"] }

html_static_path = ["_static"]
html_css_files = ["custom.css"]

# ---------------------------------------------------------------------------- #
# extension configuration -- sphinx.ext.extlinks

extlinks = {

    "repo-dir": (
        "https://gitlab.lsd.di.uminho.pt/albertofaria/bdus/tree/{}/%s"
            .format(version),
        ""
        ),

    "repo-file": (
        "https://gitlab.lsd.di.uminho.pt/albertofaria/bdus/blob/{}/%s"
            .format(version),
        ""
        ),

    "diff": (
        "https://gitlab.lsd.di.uminho.pt/albertofaria/bdus/compare/%s",
        ""
        ),

}

# ---------------------------------------------------------------------------- #
# extension configuration -- breathe

breathe_default_project = "libbdus"

breathe_projects = {
    "libbdus": "_build/breathe/doxygen/libbdus/xml",
    "kbdus": "_build/breathe/doxygen/kbdus/xml",
    }

breathe_projects_source = {
    "libbdus": ("../libbdus/include", ["bdus.h"]),
    "kbdus": ("../kbdus/include", ["kbdus.h"]),
    }

breathe_domain_by_extension = { "h": "c" }
breathe_default_members     = ("members", "undoc-members")

breathe_doxygen_config_options = {
    "HIDE_SCOPE_NAMES": "YES", # fixes rendering problems for func. ptr. fields
    "WARN_NO_PARAMDOC": "YES",
    }

breathe_show_define_initializer = True

# ---------------------------------------------------------------------------- #
# extension configuration -- versionwarning.extension

versionwarning_messages = {
    "latest": (
        "You are reading the documentation for BDUS' development version."
        " <a href=\"/en/stable\">Click here for the newest stable release.</a>"
        ),
}

versionwarning_default_message = (
    "You are reading the documentation for BDUS {version}."
    " The newest stable release is {{newest}}."
    ).format(version=version)

versionwarning_body_selector = "body"
versionwarning_banner_html = "<div id=\"version-warning-banner\">{message}</div>"

# ---------------------------------------------------------------------------- #
