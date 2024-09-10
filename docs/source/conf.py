# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Path setup --------------------------------------------------------------

# run doxygen (using the Doxyfile in this directory) to make xml
import subprocess
subprocess.call('doxygen', shell=True)


# -- Project information -----------------------------------------------------

project = 'libvxsdr'
copyright = '2024, Vesperix Corporation'
author = 'Vesperix Corporation'

# The full version, including alpha/beta/rc tags
release = '0.19.7'

# -- General configuration ---------------------------------------------------

# Add any Sphinx extension module names here, as strings.

extensions = [ 'sphinx.ext.mathjax', 'breathe' ]

breathe_projects = { 'vxsdr' : '../xml/' }

breathe_default_project = 'vxsdr'

# Add any paths that contain templates here, relative to this directory.
templates_path = ['_templates']

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = []


# -- Options for HTML output -------------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#

html_theme = 'furo'
html_theme_options = {

}

latex_engine = 'pdflatex'

#custom file to allow tweaking
html_css_files = [
    'custom.css',
]

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory.

html_static_path = ['_static']
html_logo = '_static/vesperix_logo.png'
