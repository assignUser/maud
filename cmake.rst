.. _cmake:

=====
CMake
=====

``Maud`` generates a ``CMakeLists.txt`` which is simple enough to be in
`.gitignore <https://github.com/bkietz/maud/blob/58453ba/.gitignore#L6>`_
(but you can also modify it/check it in if you like).
The generated ``CMakeLists.txt`` is packed with features, including automatic
discovery of your source files and inferrence of targets and linkage from
their contents.

Auto-inclusion
--------------

Whenever explicit configuration is necessary, ``.cmake`` modules may be included
anywhere in the source tree- close to the portion of the project they affect.
``.cmake`` modules are detected and automatically included by ``CMakeLists.txt``.
These might be used to:

- declare project :ref:`options`
- specify additional files to be :cmake:`installed <command/install.html>`
- generate source files
- set source file properties, for example :ref:`MAUD_PREPROCESSING_SCAN_OPTIONS <maud-preprocessing-scan-options>`
- use ``find_package()`` or ``FetchContent`` to set up non-maud dependencies

Auto-included modules have access to:

``${MAUD_DIR}``
  (aka ``${CMAKE_BINARY_DIR}/_maud``) a directory into which
  maud-specific build files will be written.

:ref:`glob() <glob-function>`
  which produces a list of matching files.

``${dir}``
  the directory containing the current auto-included cmake module
  (a convenience alias for ``${CMAKE_CURRENT_LIST_DIR}``)

:ref:`option() <option-function>`
  which extends cmake's built-in build option declarations.

``string_escape()``
  escapes a string for inclusion in C or json.


``cmake_modules`` directories
-----------------------------

``.cmake`` modules for which greater control of inclusion is required can be 
placed in directories named ``cmake_modules``. Any ``cmake_modules`` directories
will be added to :cmake:`CMAKE_MODULE_PATH <variable/CMAKE_MODULE_PATH.html>`
and their contents will be available for explicit inclusion, including by
auto-included modules. These directories might contain:

- Declaration of a cluster of related project :ref:`options`
  referenced by multiple other cmake modules, grouped for clarity
- A ``Find<PackgeName>.cmake`` script for use with
  :cmake:`find_package() <command/find_package.html>`

Use of :cmake:`include_guard() <command/include_guard.html>` is strongly
recommended to ensure that explicitly included modules are only included once.

