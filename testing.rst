Unit tests
----------

While scanning modules, ``Maud`` will detect and
:cmake:`add <command/add_test.html>` unit tests.
(This can be disabled by setting ``BUILD_TESTING = OFF``.)
Unit testing is based on :gtest:`GTest </>`, and many basic
concepts like suites of test cases are inherited whole.
Many other aspects of writing tests are simplified.
Instead of duplicating GTest's documentation or explaining
unit tests from the ground up, this documentation will
assume familiarity and mostly describe the ways ``Maud``'s
usage differs.

Instead of defining test suites explicitly with classes,
one test suite is produced for each C++ source which includes
the special module declaration ``module test_``. Each test suite
is compiled into an executable target named ``test_.${SUITE_NAME}``.
In a suite source file the macros ``TEST_``, ``EXPECT_``, and ``SUITE_``
respectively are used to define test cases, make assertions, and
specify share resources across the suite.
(These macros are included in the predefines buffer so
an explicit ``#include`` is unnecessary.)

``TEST_(case_name, parameters...) { ...test body... }``
    Defines and registers a test case with optional parameters.

    If no parameters are provided, a single
    :gtest:`simple test case <primer.html#simple-tests>` is defined.
    If parameters are provided, each is wrapped into a distinct
    test case using the same test body. In the scope of the test body,
    the parameter is declared as ``Parameter const &parameter``.

    If parameters are read from
    an initializer list or other range then this is analogous to a
    :gtest:`value parametrized test<advanced.html#value-parameterized-tests>`.
    Parameters may also differ in type if they are read from a
    tuple, analogous to a
    :gtest:`type parametrized test<advanced.html#typed-tests>`.

    Each parameter is
    :gtest:`printed <advanced.html#teaching-googletest-how-to-print-your-values>`
    and incorporated into the test case's total name along with
    ``case_name`` and the suite's name to make it accessible to
    :gtest:`filtering <advanced.html#running-a-subset-of-the-tests>`.

``EXPECT_(condition); EXPECT_(l <=> r); EXPECT_(e >>= matcher);``
    Checks its condition, producing a failure if it is falsy.
    To provide more information about a failed expectation, the
    condition will be printed as part of the failure. If the
    condition is a comparison, each argument will be printed.

    ``operator>>=`` is overloaded for use with
    :gtest:`matchers <reference/matchers.html>`. (All matchers
    provided by GMock are exported by ``test_`` and so are
    available in a test suite without an explicit ``#include``.)

``SUITE_ { ...shared suite state... };``
    Defines a ``struct`` which will be constructed once before
    any cases in the suite are run and destroyed when no more
    cases from the suite will run.

    (Constructed/destroyed in
    :gtest:`SetUpTestSuite/TearDownTestSuite <advanced.html#sharing-resources-between-tests-in-the-same-test-suite>`,
    respectively.)

    This may be omitted, in which case no state will be shared.
    If it is provided it must precede all ``TEST_`` definitions
    (this is checked at runtime).

    A pointer to the constructed state ``struct`` is accessible
    in test bodies by calling ``suite_state()``.


.. FIXME GTest is not easily includable yet

GTest is added to the include path for the suite, so explicit
``#include <gtest/gtest.h>`` is always available if necessary.
Each suite is linked to ``gtest_main``. Since that defines ``main``
as a weak symbol, a custom main function can be written in a
test suite. To write a custom main function for all test suites,
write an interface unit with ``export module test_:main;`` and
that will replace ``gtest_main``.

Example
=======

.. code-block:: c++

    SUITE_ { std::string yo = "yo"; };

    TEST_(basic) {
      int three = 3, five = 5;
      EXPECT_(three == five);
      // ~/maud/.build/_maud/project_tests/unit testing/basics.cxx:12: Failure
      // Expected: three == five
      //   Actual:     3 vs 5
      EXPECT_(not three);
      // ~/maud/.build/_maud/project_tests/unit testing/basics.cxx:14: Failure
      // Expected: three
      //     to be false

      // EXPECT_(...) is an expression contextually convertible to bool
      if (not EXPECT_(&three != nullptr)) return;
      EXPECT_(&three != nullptr) or [](std::ostream &os) {
        // A lambda can hook expectation failure and add more context
      };

      EXPECT_(suite_state()->yo == "yo");

      // GMock's matchers are available
      EXPECT_("hello world" >>= HasSubstr("llo"));
    }

    // To check a test body against multiple values, parametrize with a range.
    TEST_(parameterized, {111, 234}) {
      EXPECT_(parameter > 0);
    }

    // To instantiate the test body with multiple types, parametrize with a tuple.
    TEST_(typed, 0, std::string("")) {
      EXPECT_(parameter + parameter == parameter);
    }

Overriding ``test_``
====================

If it is preferable to override ``test_`` entirely (for
example to use a different test library like
`Catch2 <https://github.com/catchorg/Catch2/tree/devel/docs>`_
instead of ``GTest``), write an interface unit with
``export module test_``> and define the cmake function ``maud_add_test``:

.. code-block:: cmake

  maud_add_test(source_file partition out_target_name)

If defined, each source file which declares ``module test_``
or a partition of it will be passed to this function and
added to the target it names. (See project test
``custom unit testing`` for an example.)

.. configuration::
    # FIXME what if index.rst's configuration doesn't go first?
    extlinks = {
        **(extlinks if 'extlinks' in globals() else {}),
        "gtest": ("https://google.github.io/googletest/%s", None)
    }
