import os

_repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

project = "libjoybus"
author = "James Smith"
copyright = "2026, James Smith"

extensions = [
    "hawkmoth",
    "hawkmoth.ext.javadoc",
    "myst_parser",
]

# Hawkmoth: parse C headers with libclang.
hawkmoth_root = _repo_root
hawkmoth_clang = [
    "-I" + os.path.join(_repo_root, "include"),
    "-std=c11",
]

# Interpret existing Doxygen/Javadoc comments (@param, @return, ...).
hawkmoth_transform_default = "javadoc"

html_theme = "furo"
html_title = "libjoybus"

exclude_patterns = ["_build", ".venv"]

numfig = True
myst_heading_anchors = 3
