Documentation
-------------

If detected, Sphinx and Doxygen will be used to build HTML documentation
from the glob of all ``.rst`` files. Sphinx is usually configured with a
separate ``conf.py`` script. Maud provides a directive to specify
configuration inline

.. code-block:: rst

  .. configuration::
    html_theme = 'pydata_sphinx_theme'

ReStructuredText files can also be rendered from ``.in2`` templates,
which allows documentation to have easy access to arbitrary cmake state.
