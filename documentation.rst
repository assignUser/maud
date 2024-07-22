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

ReStructuredText files can also be rendered from ``.in2`` templates,
which allows documentation to have easy access to arbitrary cmake state.

If detected `Doxygen <https://www.doxygen.nl/>`_ will be used to collect
apidoc and expose it for use in sphinx using `Breathe <https://www.breathe-doc.org/>`_.
