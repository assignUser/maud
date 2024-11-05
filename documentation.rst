.. _documentation:

Documentation
-------------

By default, `Sphinx <https://www.sphinx-doc.org/>`_
will be used to build documentation from all ``.rst`` files.
A Sphinx build directory for each :sphinx:`builder <builders>`
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
named ``sphinx_configuration/`` anywhere in your project. In a Maud project
``conf.py`` has access to the entire cmake ``CACHE``, including all
:ref:`options` defined in cmake. For example,
``option(ENABLE_DIAGRAMS)`` might be used in ``conf.py``:

.. code-block:: python

   import maud
   if maud.cache.ENABLE_DIAGRAMS:
       extensions += ['awesome-diagrams-ext']

... or ``option(DOCUMENT_EXPERIMENTAL)`` might be used with
:sphinx:`ifconfig <extensions/ifconfig.html>`:

.. code-block:: rst

  .. ifconfig:: maud.cache.DOCUMENT_EXPERIMENTAL

    .. experimental features doc

.. TODO talk about requirements.txt, venv, ...


API doc
=======

``Maud`` includes a Sphinx extension called ``trike`` which scans
C++ sources and headers
for ``///`` comments. `libclang <https://libclang.readthedocs.io/>`_
is used to associate these with declarations. These can then be
referenced using the ``.. trike-put::`` directive.

For example, given the following C++ and rst sources in your project:

.. code-block:: c++

  /// Frobnicates the :cpp:var:`whatsit` register.
  void frobnicate();

.. code-block:: rst

  .. trike-put:: cpp:function frobnicate

... a :sphinx:`cpp:function <domains/cpp.html#directive-cpp-function>`
directive will be introduced with content drawn from the ``///`` comment.

The content of ``///`` comments is interpreted as ReStructuredText, so
they can be as expressive as the rest of your documentation. Of particular
note for those who have used other apidoc systems: cross references from
``///`` comments to labels defined in .rst will just work.


.. TODO if there's an example of ``.rst.in2`` which isn't completely
   redundant put that here
