/// FIXME add nice docstrings here
#define TEST_(name, ...)                                              \
  struct name : Registrar<struct name> {                              \
    name() : name::Registrar{SUITE_NAME, #name, __FILE__, __LINE__} { \
      with_parameters(__VA_ARGS__);                                   \
    }                                                                 \
    template <typename Parameter>                                     \
    void body(Parameter const &);                                     \
  } name;                                                             \
  template <typename Parameter>                                       \
  void name::body(Parameter const &parameter)

#define EXPECT_(...)                                                       \
  ::expect_helper::Expectation {                                           \
    __FILE__, __LINE__,                                                    \
        ::expect_helper::Begin{} <= __VA_ARGS__ <<= ::expect_helper::End { \
      #__VA_ARGS__                                                         \
    }                                                                      \
  }
