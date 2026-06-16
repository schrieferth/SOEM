# Configuration file for the Sphinx documentation builder.
#
# Standalone documentation for this SOEM fork: the Darwin/macOS port and the
# external-process SOEM HTTP gateway. The structure mirrors the Testknecht
# documentation (index + concepts/ + adr/ plus subject sections) but this tree
# documents only this repository and does not import any Python package.
#
# Usage:
#   cd docs
#   make html

project = "SOEM (Darwin port + gateway)"
copyright = "2026, Thomas Schriefer"
author = "Thomas Schriefer"

# The documentation root is this `docs/` directory; index.md is the master page.
root_doc = "index"

extensions = [
    "myst_parser",
    "sphinx_copybutton",
    "sphinxcontrib.mermaid",
    "sphinx.ext.todo",
]

# Allow rich Markdown and stable heading anchors for cross-references.
myst_heading_anchors = 4

templates_path = []
exclude_patterns = [
    "_build",
    "Thumbs.db",
    ".DS_Store",
]

html_theme = "sphinx_rtd_theme"
html_static_path = []
html_last_updated_fmt = "%Y_%m_%d_%Hh%Mm%Ss"
