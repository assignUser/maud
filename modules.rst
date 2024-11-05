.. TODO introduction to C++20 modules

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

