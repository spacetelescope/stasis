# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Path setup --------------------------------------------------------------

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
#
# import os
# import sys
# sys.path.insert(0, os.path.abspath('.'))
import os
import shutil
import subprocess
import sys

print("current directory: {}".format(os.path.abspath(os.curdir)), file=sys.stderr)

if os.path.exists("html"):
    shutil.rmtree("html")

# Update doxygen config
#subprocess.run("doxygen -u", shell=True)

# Run doxygen
subprocess.run("doxygen", shell=True)


# -- Project information -----------------------------------------------------

project = 'Oh My Cal'
copyright = '2023-2024, Space Telescope Science Institute'
author = 'Joseph Hunkeler'

root_doc = "nop"
html_extra_path = ["html"]

# -- General configuration ---------------------------------------------------

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']
