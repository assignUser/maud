.. _in2-templates:

``.in2`` Templates
------------------

``.in2`` files are automatically globbed up and their templates rendered.
The template file ``${CMAKE_SOURCE_DIR}/dir/f.txt.in2`` will be rendered to
``${MAUD_DIR}/rendered/dir/f.txt``. Since globs are also be applied to files in
``${MAUD_DIR}/rendered``, rendered source files and headers will be included in
the build automatically.

Template files are compiled to cmake modules which render the template on inclusion.
As such they have access to all the capabilities of a cmake module, including
calling arbitrary commands. Rendering uses a dedicated scope, so ``set()`` will not
affect the enclosing scope (unless ``PARENT_SCOPE`` is specified, but are you *sure* you
want to do that?) In addition to everything available to auto-included cmake modules, the
following variables are available inside a template file:

- ``${RENDER_PATH}`` the path to which the template file will be rendered.
  It is a relative to ``${MAUD_DIR}/rendered``. A template file can also override
  its output path by overwriting this variable.

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

.. code-block:: c++.in2

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

Pipeline syntax
===============

For additional syntactic sugar in the common case of modifying a
value before rendering, pipeline syntax is also supported

.. code-block:: c++.in2

  #define FOO_ENABLED @FOO_ENABLED | if_else(1 0)@
  // renders to
  #define FOO_ENABLED 1

.. TODO document all filters

.. TODO doc foreach filter

Template filters are cmake commands prefixed with ``template_filter_``.
They are assumed to read and then overwrite the variable ``${IT}``.
Whatever value ``${IT}`` has at the end of the pipeline is what gets
rendered. For example, the filter ``if_else`` is implemented with

.. code-block:: cmake

  function(template_filter_if_else then otherwise)
    if(IT)
      set(IT "${then}" PARENT_SCOPE)
    else()
      set(IT "${otherwise}" PARENT_SCOPE)
    endif()
  endfunction()
