Maud
====

A low configuration convention for cmake-built C++ modules.

- Do not require enumeration of source files; use globs.
- Do not require specification of targets; infer these from exported modules.
- Do not require finding/linking to libraries; infer these from imported modules.
- Do not require install manifests; generate and install what's necessary.
- Do not lock authors into learning maud equivalents for what's already
  available in cmake; reverting to configuration is easy.
- Do not promise dependency management; there is no single best answer to this
  (and there are plenty of okay-ish answers, and a few people stuck with poor ones).

Globbing
--------

All globs are executed from the root of version control. Changes in the set of
files yielded by any of the globs described below will cause cmake to rerun.

To briefly summarize, globs are used to find:

- ``cmake_modules`` directories, which are added to the module path
- ``.cmake`` modules, which are included
- ``.in2`` template files, which are rendered
- ``include`` directories, which are added to the include path
- C++ source files, which are scanned and added to targets

By default the extensions used to identify C++ source files are
``.cxx .cxxm .ixx .mxx .cpp .cppm .cc .ccm .c++ .c++m``.
These can be customized by setting the variable ``MAUD_CXX_SOURCE_EXTENSIONS``
at build generation time.

By default, directories named ``build``, ``foo-build``, ``.bar``, or ``_baz`` will
be excluded from globbing. This can be adjusted with the
``MAUD_IGNORED_SOURCE_REGEX`` variable. Build directories *MUST* be excluded;
if they are detected in any glob then build generation will terminate.

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
- ``MaudTemplateFilters.cmake`` a module of filters for use in template files.
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

The primary module interface unit is required to ``export import``
every partition which is a module interface unit `CXX(20:module.unit#3)
<https://timsong-cpp.github.io/cppwp/n4868/module.unit#3>`_, and if you have
written partitions then you probably don't have anything in the primary
module interface unit except those ``export import`` declarations. This feels
boilerplate-y, so if no primary module interface unit is detected then one will
be generated containing just those ``export import`` declarations.

Questionable support:
~~~~~~~~~~~~~~~~~~~~~

- Translation units other than module interface units are not necessarily reachable:
  `CXX(20:module.reach#1) <https://timsong-cpp.github.io/cppwp/n4861/module.reach#1>`_
  Importing translation units other than necessarily reachable ones is implementation
  defined. For example this includes importing a partition which is not an interface
  unit.
- As of this writing GCC 14 does not support ``module:private``.
- Header units are not currently supported.
- ``import std`` might be supported by your compiler, but maud does not guarantee it.

Options
-------

Maud overloads the built-in ``option()`` function (backwards-compatibly) to provide
support for more sophisticated configuration options. For example:

.. code-block:: cmake

  set(OPTION_GROUP "Foo-related options")
  option(
    BOOL FOO_EMULATED
    HELP "Emulate FOO functionality rather than requesting a real FOO endpoint."
    REQUIRES
      BUILD_SHARED_LIBS ON
      # If FOO_EMULATED=ON, BUILD_SHARED_LIBS will be set to ON
  )
  option(
    (LOW MED HI) FOO_LEVEL
    HELP "What level of FOO API should be requested."
    REQUIRES
    IF HI
      FOO_EMULATED OFF
  )
  resolve_options()

This declares two options which can be specified during configuration (via ``-D``
command line arguments, environment variables, guis, etc). ``BOOL`` options as
well as ``PATH``, ``STRING``, and ``ENUM`` arguments may be provided. Values provided
for ``BOOL`` and ``ENUM`` options are validated automatically to be in ``OFF;ON`` or
from their explicit set, respectively. Other options may specify a block of code
in the ``VALIDATE`` argument which will be evaluated when the option's value is
resolved. Groups of associated options can be specified by assigning to the
``OPTION_GROUP`` variable.

Option values are frequently interdependent; for example enabling one feature
might be impossible without enabling its dependencies. ``option()`` supports this
through the ``REQUIRES`` block. In this block the requirements of each option can
be specified in terms of assignments to other options on which it depends. After
all options are declared, ``resolve_options()`` assigns values to each option
ensuring all requirements are met (or reporting an error if cyclic dependencies
have been declared). Note that user provided values will always be overridden
if necessary to satisfy option requirements. On a fresh configuration it is
possible to detect this and a warning will be issued to facilitate avoidance of
inconsistent user provided values.

``resolve_options()`` also prints a grouped report of the final value of each
option, along with the reason for its value and the ``HELP`` string.
Multiline ``HELP`` strings are supported for this report. Note that
``CMakeCache.txt`` only supports single line helpstrings so in ``ccmake`` and other
applications which view the cache directly only the first line will appear.

.. code-block::

  -- FOO-related options:
  -- 
  -- FOO_EMULATED = OFF [constrained by FOO_LEVEL]
  --      Emulate FOO functionality rather than requesting a real FOO endpoint.
  -- FOO_LEVEL: ENUM(LOW MED HI) = HI [user configured]
  --      What level of FOO API should be requested.

Each call to ``resolve_option()`` also saves a cmake configure preset to
``CMakeUserPresets.json`` for easy copy-pasting, reproduction, etc. (These are
initially named with the timestamp of their creation.) Finally, each option
is surfaced in every C++ source file as a predefined macro:

.. code-block:: cpp

  /*! Emulate FOO functionality rather than requesting a real FOO endpoint. */
  #define FOO_ENABLED 0
  /*! What level of FOO API should be requested. */
  #define FOO_LOW 0
  #define FOO_MED 0
  #define FOO_HI 1

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
`CXX(20:module.global.frag#1)
<https://timsong-cpp.github.io/cppwp/n4868/module.global.frag#1>`_,
however clang allows both.)

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

Template files
--------------

A common source of cmake boilerplate is file configuration, preprocessor defines,
or otherwise passing cmake variables down to source files. To alleviate this ``.in2``
files are also globbed up and their templates rendered. The template file
``${CMAKE_SOURCE_DIR}/dir/f.txt.in2`` will be rendered to
``${MAUD_DIR}/rendered/dir/f.txt``. Subsequent globs (include directories,
C++ source files, any globs executed in an auto-included cmake module) are
additionally applied rooted at ``${MAUD_DIR}/rendered``, so rendered source files and
headers will be included in the build automatically.

Template files are compiled to cmake modules which render the template on inclusion.
As such they have access to all the capabilities of a cmake module, including
calling arbitrary commands. Rendering uses a dedicated scope, so ``set()`` will not
affect the enclosing scope unless ``PARENT_SCOPE`` is specified (are you *sure* you
want to do that?) In addition to everything available to cmake modules, the
following variables are available inside a template file:

- ``${RENDER_PATH}`` the path to which the template file will be rendered.
  It is relative to ``${MAUD_DIR}/rendered``. A template file can also override
  its output path by overwriting this variable (including to an absolute path).

- ``render(args...)`` appends its arguments into the rendered file.

- ``${IT}`` the current value in a pipeline, see below.

Template file format is intended to evoke what's accepted by ``configure_file()``.
In the most basic case, ``@VAR@`` gets replaced with ``${VAR}``'s value from cmake

.. code-block:: cpp

  #define FOO_STRING "@FOO_STRING@" // substitution of cmake variables
  #define AT_CHAR '@@'              // if you need a literal @@
  // renders to
  #define FOO_STRING "foo and bar" // substitution of cmake variables
  #define AT_CHAR '@'              // if you need a literal @

However, arbitrary commands can also be inserted between pairs of ``@``

.. code-block:: cpp

  static const char* FOO_FEATURE_NAMES[] = {@
    foreach(feature ${FOO_FEATURE_NAMES})
      render("  \"${feature}\",\n")
    endforeach()
  @};
  // renders to
  static const char* FOO_FEATURE_NAMES[] = {
    "FOO",
    "BAR",
    "BAZ",
  };

For additional syntactic sugar in the common case of modifying a
value before rendering, pipeline syntax is also supported

.. code-block:: cpp

  @include(MaudTemplateFilters)@
  #define FOO_ENABLED @FOO_ENABLED | if_else(1 0)@
  // renders to
  #define FOO_ENABLED 1

Template filters are cmake commands prefixed with ``template_filter_``.
They are assumed to read and then overwrite the variable ``${IT}``.
Whatever value ``${IT}`` has at the end of the pipeline is what gets
rendered. For example, the filter ``if_else`` is implemented with

.. code-block:: cmake

  macro(template_filter_if_else then otherwise)
    if(${IT})
      set(IT "${then}")
    else()
      set(IT "${otherwise}")
    endif()
  endmacro()

Utilities
---------

A number of C++ programs are provided:

- simple scanner

- template compiler

.. configuration::
  project = 'Maud'
  author = 'Benjamin Kietzman <bengilgit@gmail.com>'

  html_theme = 'pydata_sphinx_theme'
