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

Sphinx configuration (minimally, your ``conf.py``) should be put in a directory
named ``sphinx_configuration/`` anywhere in your project. In a Maud project,
``conf.py`` has access to all :ref:`options` defined in cmake. For example,
``option(ENABLE_DIAGRAMS)`` might be used in ``conf.py``:

.. code-block:: python

   import maud
   if maud.ENABLE_DIAGRAMS:
       # set up sphinx extension for diagrams

... or ``option(DOCUMENT_EXPERIMENTAL)`` might be used with
`ifconfig <https://www.sphinx-doc.org/en/master/extensions/ifconfig.html>`_:

.. code-block:: rst

  .. ifconfig:: maud.DOCUMENT_EXPERIMENTAL

    .. experimental features doc

.. TODO talk about import maud, requirements.txt, venv, ...


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


.. TODO if there's an example of ``.rst.in2`` which isn't completely
   redundant put that here
