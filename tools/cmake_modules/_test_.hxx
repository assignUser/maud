/// FIXME add nice docstrings here
#define TEST_(name, ...)                                                                \
  struct SUITE_NAME##_##name : Registrar<struct SUITE_NAME> {                           \
    SUITE_NAME##_##name() : SUITE_NAME##_##name::Registrar{__FILE__, __LINE__, #name} { \
      with_parameters<struct SUITE_NAME##_##name>(__VA_ARGS__);                         \
    }                                                                                   \
    template <typename Parameter>                                                       \
    static void body(Parameter const &);                                                \
  } SUITE_NAME##_##name;                                                                \
  template <typename Parameter>                                                         \
  void SUITE_NAME##_##name::body(Parameter const &parameter)

#define EXPECT_(...)                                                       \
  ::expect_helper::Expectation {                                           \
    __FILE__, __LINE__,                                                    \
        ::expect_helper::Begin{} <= __VA_ARGS__ <<= ::expect_helper::End { \
      #__VA_ARGS__                                                         \
    }                                                                      \
  }

#define SUITE_STATE struct SUITE_NAME
