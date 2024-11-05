====
Maud
====

A low configuration convention for C++ projects.

``Maud`` is built on :cmake:`CMake </>`, but works hard to eliminate
boilerplate. For simple projects, **no** hand written ``cmake`` is required.
Whenever explicit configuration becomes necessary, minimal and focused ``cmake``
can be written wherever makes the most sense for your project.
Read more about :ref:`cmake convention in Maud <cmake>`.

Globbing
--------

``Maud`` extends CMake's built in globbing support with more expressive
patterns, support for exclusion as well as inclusion, and greater performance.
Read more about :ref:`globbing <globbing-case>`.

To briefly summarize, globs are used to find:

- ``cmake_modules`` directories, which are added to the module path
- ``.cmake`` modules, which are automatically included
- ``.in2`` template files, which are rendered
- ``include`` directories, which are added to the include path
- C++ source files, which are scanned for modules and automatically
  attached to build targets

Targets
-------

The executables, libraries, and tests defined by a project are inferred from
scans of C++ sources. Read more about :ref:`automatic targets <targets>`.

More sophisticated options
--------------------------

Maud backwards-compatibly overloads the built-in
:cmake:`option <command/option.html>` function to provide
support for more sophisticated
configuration options:

- uniform declaration for all types of option
- resolution of interdependent option values
- easy access to options in C++ as predefined macros
- clean summarization of all options, complete with multiline help strings
- serialization to :cmake:`preset JSON <manual/cmake-presets.7.html#configure-preset>`
  for repeatability

.. code-block:: cmake

  option(
    FOO_LEVEL
      ENUM LOW MED HI
    "
    What level of FOO API should be requested.
    LOW is primarily used for testing and is not otherwise recommended.
    "
    DEFAULT MED

    REQUIRES
    IF HI
      # LOW or MED levels can be emulated but HI requires a physical FOO endpoint.
      FOO_EMULATED OFF

    ADD_COMPILE_DEFINITIONS
  )

Read more about :ref:`options`.

Preprocessing
-------------

By default, maud uses a custom module scanner which ignores preprocessing
for efficiency and stops reading source files after the import declarations.
This works in the most common case where the preprocessor only encounters
``#include`` directives and an occasional ``#define``, which leaves
the module dependency graph unaffected. However it is possible for the
preprocessor to affect module and import declarations. For example:

- an import declaration could be inside a conditional preprocessing block

.. code-block:: cpp

  module foo;
  #if BAR_VERSION >= 3
  import bar;
  #endif

- a set of import declarations could be included

.. code-block:: cpp

  module foo;
  #include "common_imports.hxx"

- a macro could expand to a pragma directive which modifies an ``#include``

.. code-block:: cpp

  module;
  #include "macros.hxx"
  PUSH_INGORED_WARNING(-Wunused-variable);
  #include "dodgy.hxx"
  POP_INGORED_WARNING();
  module foo;

- a macro could be used to derive the name of the module

.. code-block:: cpp

  module;
  #include "macros.hxx"
  module PP_CAT(foo_, FOO_VERSION);

(I'm actually not sure that the last two are even legal since a global
module fragment should exclusively contain preprocessing directives
:cxx20:`module.global.frag#1`, however clang allows both.)

IMHO, it is not desirable to write interface blocks which depend on preprocessing.
Moreover C++26 will restrict usage of the preprocessor severely in module declarations
as described in `P3034R1 <https://isocpp.org/files/papers/P3034R1.html>`_.

.. _maud-preprocessing-scan-options:

For source files which require it, the property ``MAUD_PREPROCESSING_SCAN_OPTIONS``
can be set in cmake. This property should contain all compile options
necessary to correctly preprocess the source file, for example
``-I /home/i/foo/include -isystem /home/i/boost/include -DFOO_ENABLE_BAR=1``.

Note that the output of these tools is in the JSON format described by `p1689
<https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p1689r5.html>`_
and does not distinguish between a module implementation unit of ``foo`` from a
source file which happens to import ``foo``. Without information about which module
an implementation unit is associated with, it cannot be automatically added to
the corresponding target. As a workaround if you must have a preprocessing scan
of an implementation unit, you can split the implementation unit into partitions
whose primary module is exposed.

Built-in support for generated files
------------------------------------

A common source of cmake boilerplate is wiring up rendering of template files,
running schema compilers, and otherwise generating code. ``Maud`` provides a
single build subdirectory for these files to land in and natively supports
including them in any file set: all globs will include matching files in
``${MAUD_DIR}/rendered`` as well as those in ``${CMAKE_SOURCE_DIR}``
(unless :ref:`explicitly excluded <glob-function-exclude_rendered>`).

Additionally, projects using ``Maud`` can use a built-in
:ref:`template format <in2-templates>` inspired by ``configure_file()``
to smoothly render configuration information into generated code.
If the template file ``${CMAKE_SOURCE_DIR}/dir/foo.cxx.in2`` exists,
it will automatically be rendered to ``${MAUD_DIR}/rendered/dir/foo.cxx``
and included in compilation alongside non-generated C++:

.. code-block:: c++.in2

  #define FOO_ENABLED @FOO_ENABLED | if_else(1 0)@
  // renders to
  #define FOO_ENABLED 1

Super easy documentation
------------------------

If a Python3 interpreter is found, Sphinx will be used to build documentation
from the glob of all ``.rst`` files. Read more about :ref:`documentation`.

Utilities
---------

A number of C++ programs are provided:

- simple scanner

- template compiler

Table of Contents
-----------------

.. toctree::
  :glob:

  *
