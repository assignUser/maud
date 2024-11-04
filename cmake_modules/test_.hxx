// clang-format off

#define GTEST_STRINGIFY_HELPER_(name, ...) #name
#define GTEST_STRINGIFY_(...) GTEST_STRINGIFY_HELPER_(__VA_ARGS__, )

///.. c:macro:: TEST_(case_name, parameters...)
///
/// Defines and registers a test case with optional parameters.
///
/// :param case_name: The test case's name
/// :param parameters: Parameters with which to parameterize the test body
///
/// If no parameters are provided, a single
/// :gtest:`simple test case <primer.html#simple-tests>` is defined.
///
/// .. code-block::
///
///   TEST_(basic) {
///     // assertions etc...
///   }
///
/// If parameters are provided, each is wrapped into a
/// distinct test case using the same test body. In the scope of
/// the test body, the parameter is declared as
///
/// .. var:: Parameter const &parameter
///
/// If parameters are read from an initializer list or other
/// range then this is analogous to a
/// :gtest:`value parameterized test <advanced.html#value-parameterized-tests>`.
///
/// .. code-block::
///
///   TEST_(value_parameterized, {2, 3, 47, 8191}) {
///     EXPECT_(is_prime(parameter));
///   }
///
/// Parameters may also differ in type if they are read from a tuple,
/// analogous to a
/// :gtest:`type parameterized test <advanced.html#type-parameterized-tests>`.
///
/// .. code-block::
///
///   TEST_(type_parameterized, 0, std::string("")) {
///     EXPECT_(parameter + parameter == parameter);
///   }
///
/// Each parameter is
/// :gtest:`printed <advanced.html#teaching-googletest-how-to-print-your-values>`
/// and incorporated into the test case’s total name along with case_name and the
/// suite’s name to make it accessible to
/// :gtest:`filtering <advanced.html#running-a-subset-of-the-tests>`.
#define TEST_(case_name, ...)                                            \
  namespace SUITE_NAME {                                                 \
  struct case_name : Registrar<struct SuiteState> {                      \
    case_name() {                                                        \
      register_(this, {__FILE__, __LINE__, GTEST_STRINGIFY_(SUITE_NAME), \
                       #case_name} __VA_OPT__(, ) __VA_ARGS__);          \
    }                                                                    \
    template <typename Parameter>                                        \
    static void body(Parameter const &parameter);                        \
  } case_name;                                                           \
  }                                                                      \
  template <typename Parameter>                                          \
  void SUITE_NAME::case_name::body(Parameter const &parameter)

///.. c:macro:: EXPECT_(condition...)
///
/// Checks its condition, producing a failure if it is falsy.
///
/// :param condition: An expression which is expected to be truthy.
///
/// To provide more information about a failed expectation, the
/// condition will be printed as part of the failure. If the
/// condition is a comparison, each argument will be printed.
///
/// .. code-block::
///
///   int three = 3, five = 5;
///   EXPECT_(three == five);
///   // ~/maud/.build/_maud/project_tests/unit testing/basics.cxx:12: Failure
///   // Expected: three == five
///   //   Actual:     3 vs 5
///
/// :expr:`EXPECT_(...)` produces an expression rather than a statement.
/// It is contextually convertible to ``bool``, truthy iff the condition
/// was truthy. If additional context needs to be added to a failed
/// expectation, a lambda can be provided which will only be called
/// if the expectation fails.
///
/// .. code-block::
///
///   EXPECT_(&a == &b) or [&](auto &os) {
///     os << "Extra context: " << a << " vs " << b;
///   };
///
/// :gtest:`Matchers <reference/matchers.html>` can also be used to write an
/// assertion with ``EXPECT_`` through use of an overloaded ``operator>>=``.
/// (All matchers provided by GMock are exported by ``test_`` and so are
/// available in a test suite without an explicit ``#include``.)
///
/// .. code-block::
///
///   auto str  = "hello world";
///   EXPECT_(str >>= HasSubstr("boo"));
///   // ~/maud/.build/_maud/project_tests/unit testing/basics.cxx:21: Failure
///   // Expected: str has substring "boo"
///   // Argument was: "hello world"
#define EXPECT_(...)                                                                  \
  ::expect_helper::Expectation {                                                      \
    __FILE__, __LINE__,                                                               \
        (::expect_helper::Begin{} <= __VA_ARGS__, ::expect_helper::End{#__VA_ARGS__}) \
  }

/// Define state/resources available during a suite's execution.
///
/// Defines a ``struct`` which will be constructed once before
/// any cases in the suite are run and destroyed when no more
/// cases from the suite will run. (Constructed/destroyed in
/// :gtest:`SetUpTestSuite/TearDownTestSuite <advanced.html#sharing-resources-between-tests-in-the-same-test-suite>`
/// respectively.)
///
/// .. code-block::
///
///   SUITE_ {
///     SuiteState() {
///       server_handle.connect_to("localhost", 7890);
///       EXPECT_(server_handle.is_connected());
///     }
///     ~SuiteState() {
///       server_handle.orderly_shutdown();
///       EXPECT_(not server_handle.is_connected());
///     }
///     ServerHandle server_handle;
///   };
///
/// This may be omitted, in which case no state will be shared.
/// If it is provided it must precede all :expr:`TEST_(...)`
/// definitions (this is checked at runtime).
///
/// A pointer to the constructed state ``struct`` is accessible
/// in test bodies by calling :expr:`suite_state()`.
///
/// .. code-block::
///
///   TEST_(address) {
///     EXPECT_(suite_state()->server_handle.address() == "localhost:7890");
///   }
#define SUITE_           \
  namespace SUITE_NAME { \
  struct SuiteState;     \
  }                      \
  struct SUITE_NAME::SuiteState : DontTerminateIfDestructionThrows

// clang-format on
