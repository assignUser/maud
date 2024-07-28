.. _options:

Options
-------

Options are CMake ``CACHE`` variables which parameterize a project,
augmented with optional properties to present them to users.
Option declarations are a standard interface for tuning builds;
if a feature may be configured then an option declaration is the
most accessible way to express it.

``option`` and ``resolve_options``
==================================

.. _option-function:

.. code-block:: cmake

  option(
    name    < | BOOL | PATH | FILEPATH | STRING | ENUM enum_values... >
    help_string
    [DEFAULT default_value]
    [MARK_AS_ADVANCED]
    [REQUIRES [IF condition [dependency required_value]...]...]
    [VALIDATE CODE validation_code]
    [ADD_COMPILE_DEFINITIONS]
  )

Declare an option with the provided ``name``.

Access of the option is undefined before a call to
:ref:`resolve_options <resolve_options-function>` assigns its final value.

In fresh builds, environment variables can be used to assign to options.
For example if an option named ``FOO_LEVEL`` is not otherwise defined but
``ENV{FOO_LEVEL}`` is defined, then the environment variable will be used
instead of the default. This can be disabled by setting
``$ENV{MAUD_DISABLE_ENVIRONMENT_OPTIONS} = ON``.

``< | BOOL | PATH | FILEPATH | STRING | ENUM enum_values... >``
    The :cmake:`type <prop_cache/TYPE.html>` of the ``CACHE`` variable.
    Options which are not explicitly typed are implicitly of type ``BOOL``.

    ``ENUM`` is an alias for ``STRING`` which additionally specifies allowed values
    by setting the :cmake:`STRINGS property <prop_cache/STRINGS.html>`.

``help_string``
    The :cmake:`help string <prop_cache/HELPSTRING.html>` of the ``CACHE`` variable.

    An explanation of the option's purpose, displayed in ``ccmake`` and other
    GUIs. :ref:`Predefined macros <option-compile-definitions>` for the option will
    be decorated with a doxygen-style comment containing this text (which should be
    accessible to your language server). It will also be displayed in the
    :ref:`summary <options-summary>` when configuration completes.

    .. note::

      An escaped representation of the help string is stored, which will be shown
      directly in ``ccmake`` and other viewers which only support single line
      help strings.

``DEFAULT default_value``
    The option's value if no explicit value is provided and no requirements
    constrain it.

    .. note::

      ``CACHE`` variables' values may be defined before their declaration as an
      option (for example if the option is defined on the command line via
      ``-DFOO_LEVEL=HI``) in which case the declaration will initialize other
      properties, leaving the value unchanged.

    .. list-table:: Implicit defaults

      * - ``BOOL``
        - ``OFF``
      * - ``PATH`` or ``FILEPATH``
        - ``${CMAKE_SOURCE_DIR}``
      * - ``STRING``
        - ``""``
      * - ``ENUM``
        - the first enum value

``MARK_AS_ADVANCED``
    Mark this ``CACHE`` variable :cmake:`advanced <prop_cache/ADVANCED.html>`.
    Its value will not be displayed by default in the :ref:`summary <options-summary>`
    (it will be displayed if its value is non-default) and GUIs (``ccmake`` for example
    provides an explicit toggle).

.. _requirement-block-syntax:

``REQUIRES``
    Begin a set of :ref:`requirement <option-requirements>` blocks. Each block
    begins with ``IF condition`` where ``condition`` is a possible value of the
    option. The block continues with a sequence of ``dependency required_value``
    pairs where each ``dependency`` names another option. If the option's
    value is resolved to ``condition``, then each ``dependency`` in its block
    will be set to the corresponding ``required_value``.

    .. note::

      A ``dependency`` need not be declared with ``option()`` before it is
      referenced in a requirement block, nor even before ``resolve_option()``
      would assign its value.

    .. note::

      To simplify the common case of a ``BOOL`` option which only has
      requirements when it is ``ON``, ``REQUIRES IF ON`` may be shortened to
      just ``REQUIRES``.

``VALIDATE CODE validation_code``
    Provide code to validate the option. The code block will be evaluated after
    requirements have been resolved and the option's final value is known. For
    example this could be used to assert that a ``FILEPATH`` option specifies a
    readable file.

    ``BOOL`` options are automatically validated to be either ``ON`` or ``OFF``.
    ``ENUM`` options are automatically checked against their value set.

.. _option-compile-definitions:

``ADD_COMPILE_DEFINITIONS``
    If specified, macros will be added to the predefines buffer to expose
    option values to C++ code.

    .. list-table::

      * - For a boolean option an identically named macro
          will be defined to 0 or 1

        - .. code-block:: c

            // FOO_EMULATED: BOOL
            #define FOO_EMULATED 0

      * - The name of an enumeration option will be concatenated with
          each potential value to get macro names, each of which are
          defined to 0 or 1

        - .. code-block:: c

            // FOO_LEVEL: ENUM LOW MED HI
            #define FOO_LEVEL_LOW 0
            #define FOO_LEVEL_MED 0
            #define FOO_LEVEL_HI 1

      * - For options of any other type an identically named macro will be
          defined to a string literal

        - .. code-block:: c

            // FOO_SOCKET_PATH: FILEPATH
            #define FOO_SOCKET_PATH "/var/run/foo"

.. _resolve_options-function:

.. code-block:: cmake

  resolve_options(option_names...)

Resolve option interdependencies and assign final values. If ``option_names...``
is non-empty only those options will be resolved, otherwise all unresolved
options will be resolved.

If specified, each option's custom validation code will also be evaluated.


.. _option-requirements:

Option Requirements
===================

Project options are frequently interdependent; for example enabling one feature
might be impossible without enabling its dependencies. Resolving these
interdependencies to a consistent state across all options in the project is
frequently messy and error prone.

:ref:`option() <option-function>` integrates a solution to this problem in
the :ref:`REQUIRES <requirement-block-syntax>` argument. The requirements of
each option can be specified in terms of assignments to other options on which
it depends. After options are declared,
:ref:`resolve_options() <resolve_options-function>` assigns values to declared
options and their dependencies, ensuring all requirements are met (or reporting
an error if unsatisfiable dependencies are encountered).

Options are considered to form a directed acyclic graph: each option may
declare a requirement on any other option as long as no cycles are formed.
Options with no requirements placed on them will have their default or
user configured value. Otherwise requirements determine the option's value
(even if it the dependency's default is required). Conflicting requirements
will result in failed configuration.

.. note::

  User provided values (via ``-DFOO=0`` on the command line, through preset
  JSON, from an environment variable, ...) are not considered a hard constraint
  and will always be overridden if necessary to satisfy declared requirements.
  On a fresh configuration it is possible to detect such an override and a
  warning will be issued to facilitate avoidance of inconsistent user provided
  values.

Subsets of options can be resolved before other options have been declared.
Options to be resolved can even depend on options which have not yet been declared.
New requirements can be placed on a resolved option but they will only raise
an error instead of assigning to the resolved option's value, even if the
resolved option was not constrained by a requirement block at resolution time.

.. _options-summary:

Options summary
===============

After configuration is complete, a summary of option values is printed.
The final value of each option is printed, along with the reason for that
value and the option's help string.

Groups of associated options can be declared by writing
``set(OPTION_GROUP "FOO-related options")`` before declaring the options.
This adds a heading in the summary.

.. code-block:: lua

  -- FOO-related options:
  --
  -- FOO_EMULATED = OFF [constrained by FOO_LEVEL]
  --      Emulate FOO functionality rather than requesting a real FOO endpoint.
  -- FOO_LEVEL = HI (of LOW;MED;HI) [user configured]
  --      What level of FOO API should be requested.
  --      LOW is primarily used for testing and is not otherwise recommended.
  -- FOO_SOCKET_PATH = /var/run/foo [default]
  --      Explicit socket for FOO endpoint.

.. TODO add a special target to summarize the options again

As part of the options summary, a cmake
:cmake:`configure preset <manual/cmake-presets.7.html#configure-preset>`
is appended to ``CMakeUserPresets.json`` for easy copy-pasting, reproduction,
etc. (These are initially named with the timestamp of their creation.)

Options examples
================

.. tab:: ✅ Valid

  .. code-block:: cmake

    # -Dalpha=ON
    option(alpha "" REQUIRES beta 3)
    option(
      beta ENUM 1 2 3 ""
      REQUIRES
        IF 1 gamma ON
        IF 3 gamma OFF
    )

    resolve_options()
    # no requirements on alpha, alpha resolved to ON
    # alpha=ON requires beta=3, beta resolved to 3
    # beta=3 requires gamma=OFF, gamma resolved to OFF
    #       (gamma will be declared later)

.. tab:: ❌ Unresolved

  .. code-block:: cmake
    :emphasize-lines: 10

    # -Dalpha=ON
    option(alpha "" REQUIRES beta 3)
    option(
      beta ENUM 1 2 3 ""
      REQUIRES
        IF 1 gamma ON
        IF 3 gamma OFF
    )

    if(beta EQUAL 1) # ACCESS TO UNRESOLVED OPTION
      # will not be reached; beta has not yet been resolved to 3
      setup_beta_feature()
    endif()

.. tab:: ❌ Cycle

  .. code-block:: cmake

    # -Dalpha=ON
    option(alpha "" REQUIRES beta ON)
    option(beta "" REQUIRES alpha OFF)

    resolve_options()
    # CMake Error at /tmp/usr/lib/cmake/Maud/Maud.cmake:1436 (message):
    #
    #       Circular constraint between options
    #         beta;alpha

.. tab:: ❌ Conflict

  .. code-block:: cmake

    # -Dalpha=ON -Domega=ON
    option(omega "" REQUIRES beta 1)
    option(alpha "" REQUIRES beta 3)
    option(beta ENUM 1 2 3 "")

    # CMake Error at /tmp/usr/lib/cmake/Maud/Maud.cmake:1455 (message):
    #
    #       Option constraint conflict: beta is constrained
    #       by alpha to be
    #         "3"
    #       but omega requires it to be
    #         "1"

.. tab:: ❌ Constraining resolved

  .. code-block:: cmake

    # -Dalpha=ON
    option(beta ENUM 1 2 3 "")
    resolve_options()

    if(beta EQUAL 1) # safe
      setup_beta_feature()
    endif()

    option(alpha "" REQUIRES beta 3)

    # CMake Error at /tmp/usr/lib/cmake/Maud/Maud.cmake:1468 (message):
    #
    #       Option constraint conflict: beta was already resolved to
    #         "1"
    #       but alpha requires it to be
    #         "3"
