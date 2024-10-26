Maud
====

A low configuration convention for cmake-built C++ modules.

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
- C++ source files, which are scanned for modules and added to targets accordingly

CMake
-----

In the simplest case, no hand written CMake is required. ``maud`` generates
a ``CMakeLists.txt`` which is simple enough to be ``.gitignore``'d (but you can
also modify it/check it in if you like). A glob will find your source files,
targets will be inferred from these, libraries will be linked automatically
based on what your sources import.

Optionally, ``.cmake`` modules may be scattered through the source tree.
If these are detected they will be automatically included in lexicographic
order by ``CMakeLists.txt`` before globs are executed to find source files.
These might be used to:

- generate source files
- set source file properties, for example ``MAUD_PREPROCESSING_SCAN_OPTIONS``
- use ``find_package()`` or ``FetchContent`` to set up non-maud dependencies

Auto inclusion is disabled in directories named ``cmake_modules``, but any
of these will be added to ``CMAKE_MODULE_PATH`` *before* ``.cmake`` files are
auto included. This is intended to allow control of cmake module inclusion,
for example:

- ``FindThing.cmake`` isn't intended for inclusion outside ``find_package()``
- ``VariablesUsedByTemplateFiles.cmake`` needs to be included before ``.in2`` files
  which reference the variables are rendered. The function ``include_guard()``
  is useful for the common case when a module should only be included the
  first time.

A summary of user-facing cmake variables, commands, and modules provided to
auto-included modules:

- ``${MAUD_DIR}`` (aka ``${CMAKE_BINARY_DIR}/_maud``) a directory into which
  maud-specific build files will be written.
- ``glob(out-var patterns...)`` which produces a list of matching files.
- ``${dir}`` the directory containing the current auto-included cmake module
  (see also ``${CMAKE_CURRENT_LIST_DIR}``)
- ``option()`` which extends cmake's built-in build option declarations.
- ``string_escape()`` which escapes a string for inclusion in C or json.

Targets
-------

Since targets and their dependencies are mostly inferred from modules, the first
step in generating a maud build is scanning all source files for modules provided
and imported.

- Interface units (source files with an ``export module foo;`` decl) produce
  static or shared libraries according to the value of ``BUILD_SHARED_LIBS``.

  - Interface units whose name ends in ``_`` produce non-installed ``OBJECT``
    libraries. This is useful for projects which produce multiple executables
    with shared source but do not wish to expose the shared source as a library.

- Implementation units (source files with a ``module foo;`` decl) are added as
  source files to their interface unit's library target.

- Interface or implementation units of the special ``module executable;`` produce
  executable targets. The name of the target is derived by stripping the source
  file's name of extensions - the ``STEM`` of the source file. (Do not import this
  special module.)

- Implementation units of the special ``module test_;`` produce
  a test. (Do not import this special module.) By default, this will:

  - Create an executable target for each source.
  - Pass the test executable to ``add_test()``.
  - Link the test executable to ``gtest``.

    - If an interface unit of ``module test_:main`` is found then it will be linked
      with each test executable, otherwise ``gtest_main`` will be linked.

  - If the command ``maud_add_test(source_file_path partition out_target_name)``
    is defined it will be invoked on each test source as it is scanned, allowing
    you to override what a unit test is for your project.

Other source files will not be automatically attached to any target.
Other target types require explicit cmake. For example, to produce a shared
library instead of a static library for ``module foo;`` you can write
``add_library(foo SHARED)`` in any included cmake module. This predefines the
target to which interface and implementation units of ``foo`` will be added.

Directories named ``include`` are globbed up and added to ``INCLUDE_DIRECTORIES``,
so ``$project_root/subtool/include/subtool/foo.hxx`` can be included with
``#include "subtool/foo.hxx"`` from any header or source.

Module partitions:
~~~~~~~~~~~~~~~~~~

Module partitions are a useful way to compartmentalize a module interface:

.. code-block:: cpp

  // foo-bar.cxx
  export module foo:bar;
  export int foo_bar();

  // foo-quux.cxx
  export module foo:quux;
  export int foo_quux();

  // foo.cxx
  export module foo;
  export import :bar;
  export import :quux;
  // All exports from foo:bar and foo:quux are now exported from
  // this, the primary module interface unit for foo.

The primary module interface unit is required to ``export import`` every
partition which is a module interface unit :cxx20:`module.unit#3`, and if
you have written partitions then you probably don't have anything in the primary
module interface unit except those ``export import`` declarations. This feels
boilerplate-y, so if no primary module interface unit is detected then one will
be generated containing just those ``export import`` declarations.

Questionable support:
~~~~~~~~~~~~~~~~~~~~~

- Translation units other than module interface units are not necessarily reachable:
  :cxx20:`module.reach#1`
  Importing translation units other than necessarily reachable ones is implementation
  defined. For example this includes importing a partition which is not an interface
  unit.
- As of this writing GCC 14 does not support ``module:private``.
- Header units are not currently supported.
- ``import std`` might be supported by your compiler, but maud does not guarantee it.

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

If detected, Sphinx and Doxygen will be used to build documentation
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
