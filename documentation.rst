.. _documentation:

Documentation
-------------

By default, `Sphinx <https://www.sphinx-doc.org/>`_
will be used to build documentation from all ``.rst`` files.
A Sphinx build directory for each
`builder <https://www.sphinx-doc.org/en/master/usage/builders/>`_
is staged in ``${CMAKE_BINARY_DIR}/documentation/${builder}``, and
rebuilding them is included in the ``documentation`` target:

.. code-block:: shell-session

  $ ninja -C .build documentation
  [73/76] Building dirhtml with sphinx

  $ python -m http.server -d .build/documentation/dirhtml 8000
  Serving HTTP on 0.0.0.0 port 8000 (http://0.0.0.0:8000/) ...

  $ xdg-open http://localhost:8000

Which builders are used can be controlled via ``option(SPHINX_BUILDERS)``,
which defaults to building just ``dirhtml``. To disable building
documentation, set this to an empty string.


API doc
=======

``Maud`` includes a Sphinx extension which scans C++ sources and headers
for ``///`` comments. `libclang <https://libclang.readthedocs.io/>`_
is used to associate these with declarations. These can then be
referenced using the ``.. apidoc::`` directive.

For example, given the following C++ and rst sources in your project:

.. code-block:: c++

  /// Frobnicates the :cpp:var:`whatsit` register.
  void frobnicate();

.. code-block:: rst

  .. apidoc:: frobnicate

... a
`cpp function <https://www.sphinx-doc.org/en/master/usage/domains/cpp.html#directive-cpp-function>`_
directive will be introduced with content drawn from the ``///`` comment.

The content of ``///`` comments is interpreted as ReStructuredText, so
they can be as expressive as the rest of your documentation. Of particular
note for those who have used other apidoc systems: cross references from
``///`` comments to labels defined in .rst will just work.


Configuration
=============

Sphinx is usually configured with a single ``conf.py`` script.
Maud provides a directive to specify configuration inline from ``.rst``
files:

.. code-block:: rst

  .. configuration::

    html_theme = 'furo'

``.in2`` Templates
~~~~~~~~~~~~~~~~~~

ReStructuredText files can also be rendered from ``.in2``
:ref:`templates <in2-templates>`, which allows documentation to have easy
access to arbitrary cmake state. Let's say documentation of experimental
features should be controlled by ``option(DOCUMENT_EXPERIMENTAL)``:

.. code-block:: rst

  .. ifconfig:: @DOCUMENT_EXPERIMENTAL | if_else(True False)@

    .. experimental features doc
