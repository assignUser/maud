#define GTEST_STRINGIFY_HELPER_(name, ...) #name
#define GTEST_STRINGIFY_(...) GTEST_STRINGIFY_HELPER_(__VA_ARGS__, )

/// FIXME add nice docstrings here
#define TEST_(name, ...)                                                 \
  namespace SUITE_NAME {                                                 \
  struct name : Registrar<struct SuiteState> {                           \
    name() {                                                             \
      register_(this, {__FILE__, __LINE__, GTEST_STRINGIFY_(SUITE_NAME), \
                       #name} __VA_OPT__(, ) __VA_ARGS__);               \
    }                                                                    \
    template <typename Parameter>                                        \
    static void body(Parameter const &parameter);                        \
  } name;                                                                \
  }                                                                      \
  template <typename Parameter>                                          \
  void SUITE_NAME::name::body(Parameter const &parameter)

#define EXPECT_(...)                                                                  \
  ::expect_helper::Expectation {                                                      \
    __FILE__, __LINE__,                                                               \
        (::expect_helper::Begin{} <= __VA_ARGS__, ::expect_helper::End{#__VA_ARGS__}) \
  }

#define SUITE_           \
  namespace SUITE_NAME { \
  struct SuiteState;     \
  }                      \
  struct SUITE_NAME::SuiteState
