.. _documentation:

Documentation
-------------

If detected, `Sphinx <https://www.sphinx-doc.org/>`_
will be used to build documentation from the glob of all ``.rst`` files.

Configuration
=============

Sphinx is usually configured with a single ``conf.py`` script.
Maud provides a directive to specify configuration inline from ``.rst``
files:

.. code-block:: rst

  .. configuration::

    html_theme = 'furo'

Breathe
~~~~~~~

If detected `Doxygen <https://www.doxygen.nl/>`_ will be used to collect
apidoc from C++ sources and headers. This apidoc is not automatically added
to generated documentation; instead it is exposed for use in sphinx using
`Breathe <https://www.breathe-doc.org/>`_. This allows apidoc to be presented
using Breathe's ``.. doxygenclass::`` and other directives framed with as
much or as little prose as desired.

``.in2`` Templates
~~~~~~~~~~~~~~~~~~

ReStructuredText files can also be rendered from ``.in2``
:ref:`templates <in2-templates>`, which allows documentation to have easy
access to arbitrary cmake state. For example the template ``version.rst.in2``

.. code-block:: rst

  @
  execute_process(COMMAND git tag --contains OUTPUT_VARIABLE tag)
  if(NOT tag)
    execute_process(COMMAND git rev-parse HEAD OUTPUT_VARIABLE tag)
    set(tag "git-${tag}")
  endif()
  @
  .. note:: This doc built from version ``@tag@``.

Could render to the file ``version.rst``:

.. note:: This doc built from version ``git-2f65de1c047679f8ca5d67055d42a7353b6d719d``.
