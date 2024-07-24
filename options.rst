.. _options:

Options
-------

Maud backwards-compatibly overloads the built-in
:cmake:`option <command/option.html>` function to provide
support for more sophisticated configuration options.

.. _option-function:

``option()``
============

.. code-block:: cmake

  option(
    name    < | BOOL | PATH | FILEPATH | STRING | ENUM enum_values... >
    help_string
    [DEFAULT default_value]
    [MARK_AS_ADVANCED]
    [REQUIRES [IF condition [dependency required_value]...]...]
    [VALIDATE CODE validation_code]
  )

Declare an option (``CACHE`` variable) with the provided ``name``.

Access of the option is undefined before a call to
:ref:`resolve_options <resolve_options-function>` assigns its final value.

In fresh builds, environment variables can be used to assign to options.
For example if an option named ``FOO_LEVEL`` is not specified on the command line
but ``ENV{FOO_LEVEL}`` is defined, then the environment variable will be used
instead of the default. This can be disabled by setting
``ENV{MAUD_DISABLE_ENVIRONMENT_OPTIONS}=ON``.

``< | BOOL | PATH | FILEPATH | STRING | ENUM enum_values... >``
    The :cmake:`type <prop_cache/TYPE.html>` of the ``CACHE`` variable.
    If not specified, the variable will be of type ``BOOL``.

    ``ENUM`` is an alias for ``STRING`` which additionally specifies allowed values
    by setting the :cmake:`STRINGS property <prop_cache/STRINGS.html>`.

``help_string``
    The :cmake:`help string <prop_cache/HELPSTRING.html>` of the ``CACHE`` variable.

    An explanation of the option's purpose, displayed in ``ccmake`` and other
    GUIs. :ref:`Predefined macros <option-compile-definitions>` for the option will
    be decorated with a doxygen-style comment containing this text (which should be
    accessible to your language server). It will also be displayed in the
    :ref:`summary <options-summary>` when configuration completes:

    .. code-block:: lua

      -- FOO_LEVEL = HI (of LOW;MED;HI) [user configured]
      --      What level of FOO API should be requested.
      --      LOW is primarily used for testing and is not otherwise recommended.

    .. note::
    
      An escaped representation of the help string is stored, which will be shown
      directly in ``ccmake`` and other viewers which support only a single line
      help string.

``DEFAULT default_value``
    The option's value if no explicit value is provided and no requirements
    constrain it.

    .. note::

      ``CACHE`` variables' values may be defined before their declaration as an
      option (for example if the option is defined on the command line via
      ``-DFOO_LEVEL=HI``) in which case the declaration will initialize other
      properties, leaving the value unchanged.

    If a default is not explicitly declared it will be OFF if type is ``BOOL``,
    ``CMAKE_SOURCE_DIR`` for ``PATH`` or ``FILEPATH`` options, ``""`` if type is
    ``STRING``, or the first enum value for ``ENUM`` options.

``MARK_AS_ADVANCED``
    Mark this ``CACHE`` variable :cmake:`advanced <prop_cache/ADVANCED.html>`.
    Its value will not be displayed by default in the :ref:`summary <options-summary>`
    (it will be displayed if its value is non-default) and GUIs (``ccmake`` for example
    provides an explicit toggle).

.. _requirement-block-syntax:

``REQUIRES``
    Begin a set of :ref:`requirement <option-requirements>` blocks. Each block
    begins with ``IF condition`` and continues with a sequence of
    ``dependency required_value`` pairs, where ``condition`` is a possible
    value of the option and ``dependency`` names another option. If the option's
    value is resolved to ``condition``, then each ``dependency`` will be set to 
    the corresponding ``required_value``. (Or if ``dependency``'s value is already
    constrained to some other value an error will be raised.)

    .. note::

      A ``dependency`` need not be declared with ``option()`` before it is
      referenced in a requirement block, nor even before ``resolve_option()``
      would assign its value.

    .. note::

      Since the most common option type is ``BOOL`` and the most common
      requirements pertain when it is ``ON``, the condition ``IF ON`` may
      be elided.

``VALIDATE CODE validation_code``
    Provide code to validate the option. The code block will be evaluated after
    requirements have been resolved and the option's final value is known. For
    example this could be used to assert that a ``FILEPATH`` option specifies a
    readable file.

    ``BOOL`` options are automatically validated to be either ``ON`` or ``OFF``.
    ``ENUM`` options are automatically checked against their value set.

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
declare a requirement on any other option so long as no cycles are formed.
Options with no requirements placed on them will have their default or
user configured value. Otherwise requirements determine the option's value
(even if it the dependency's default is required). Conflicting requirements
will result in failed configuration.

.. code-block:: cmake

  option(alpha "" DEFAULT ON REQUIRES beta ON epsilon ON)
  option(beta "" REQUIRES gamma OFF)
  option(gamma "" REQUIRES IF OFF delta OFF)

  # resolve_options() would FATAL_ERROR due to cyclic dependency:
  # (alpha -> beta -> gamma -> delta -> alpha)
  #option(delta "" DEFAULT IF OFF REQUIRES alpha OFF)

  # resolve_options() would FATAL_ERROR due to conflicting requirements:
  # (epsilon=ON because alpha=ON but epsilon=OFF because delta=OFF)
  #option(delta "" DEFAULT IF OFF REQUIRES epsilon OFF)

  if(beta) # access to unresolved option!
    # will not be reached since beta has not been resolved to ON yet
  endif()

  resolve_options()
  # no requirements on alpha, alpha resolved to ON
  # alpha=ON requires beta=ON, beta resolved to ON 
  # beta=ON requires gamma=OFF, gamma resolved to OFF
  # gamma=OFF requires delta=OFF, delta resolved to OFF
  # alpha=ON requires epsilon=ON, epsilon resolved to ON 

.. note::

  User provided values (via ``-DFOO=0`` on the command line, through preset
  JSON, from an environment variable, ...) are not considered a hard constraint
  and will always be overridden if necessary to satisfy declared requirements.
  On a fresh configuration it is possible to detect such an override and a
  warning will be issued to facilitate avoidance of inconsistent user provided
  values.

Subsets of options can be resolved before other options have been declared.
Options to be resolved can depend on options which have not yet been declared.
New requirements can be placed on a resolved option but they will only raise
an error instead of assigning to the resolved option's value, even if the
resolved option was not constrained by a requirement block at resolution time.

.. TODO add a special target to summarize the options again

.. _resolve_options-function:

``resolve_options()``
=====================

.. code-block:: cmake

  resolve_options(
    [ADD_COMPILE_DEFINITIONS]
    option_names...
  )

Resolve option interdependencies and assign final values. If non-empty only
``option_names...`` will be resolved, otherwise all unresolved options will be
resolved. If specified, each option's custom validation code will also run.

.. _option-compile-definitions:

``ADD_COMPILE_DEFINITIONS``
    If specified, macros will be added to the predefines buffer to expose
    resolved option values to C++ code.

    For a boolean option ``FOO_ENABLED``, an identically named macro will be
    defined to 0 or 1:

    .. code-block:: cpp

      /// Emulate FOO functionality rather than requesting a physical FOO endpoint.
      #define FOO_EMULATED 0

    For each value of an enumeration option ``FOO_LEVEL``, the name of the option and 
    the value will be concatenated to get macro names, which are defined to 0 or 1:

    .. code-block:: cpp

      /// What level of FOO API should be requested.
      /// LOW is primarily used for testing and is not otherwise recommended.
      /// (HI of LOW;MID;HI)
      #define FOO_LEVEL_LOW 0

    For options of any other type, an identically named macro will be defined to
    a string literal:

    .. code-block:: cpp

      /// Explicit socket for FOO endpoint.
      #define FOO_SOCKET_PATH "/var/run/foo"

.. _options-summary:

Options summary
===============

After configuration is complete, a summary of option values is printed.
The final value of each option is printed, along with the reason for that value and
its help string.

Groups of associated options can be declared by writing
``set(OPTION_GROUP "FOO-related options)`` before declaring the options.
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

As part of the options summary, a cmake configure preset is appended to
``CMakeUserPresets.json`` for easy copy-pasting, reproduction, etc. (These are
initially named with the timestamp of their creation.)

