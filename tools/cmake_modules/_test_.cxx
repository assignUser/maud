// leave this in cmake_modules/; it makes bootstrapping easier
// since Maud.cmake assumes that _test_.* are next to it.
module;
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <coroutine>
#include <cstdint>
#include <exception>
#include <span>
export module test_;

using namespace testing;

export template <typename T>
std::string const type_name = testing::internal::GetTypeName<T>();

template <>
auto const type_name<std::string> = "std::string";

template <>
auto const type_name<std::string_view> = "std::string_view";

export struct Main {
  Main(int &argc, char **argv) { InitGoogleTest(&argc, argv); }
  int run() { return RUN_ALL_TESTS(); }
};

template <typename Body>
class Fixture : public testing::Test, Body {
 public:
  explicit Fixture(Body body) : Body{std::move(body)} {}
  void TestBody() override { Body::operator()(); }
  // TODO provide a way to inject SetUpTestSuite. SetUp is unnecessary
};

export template <typename Test>
struct Registrar {
  char const *suite_name;
  char const *test_name;
  char const *file;
  int line;

  void register_one(char const *name, auto body) {
    testing::RegisterTest(suite_name, name, nullptr, nullptr, file, line,
                          [body = std::move(body)]() -> testing::Test * {
                            return new Fixture{std::move(body)};
                          });
  };

  void register_one(auto parameter, int i, std::string type_name = "") {
    std::string name = test_name;
    name += "/" + PrintToString(i);
    if (not type_name.empty()) {
      name += "/" + type_name;
    }
    name += "/" + PrintToString(parameter);
    register_one(name.c_str(), [this, parameter = std::move(parameter)] {
                              static_cast<Test*>(this)->body(parameter);
                            });
  };

  void with_parameters() {
    register_one(test_name, [this] { static_cast<Test*>(this)->body(nullptr); });
  }

  void with_parameter_range(auto &&range) {
    for (int i = 0; auto &&parameter : range) {
      register_one(std::move(parameter), i++);
    }
  }

  void with_parameters(auto &&parameters) {
    if constexpr (std::is_invocable_v<decltype(parameters)>) {
      with_parameter_range(std::move(parameters)());
    } else {
      with_parameter_range(std::move(parameters));
    }
  }

  template <typename... T>
  void with_parameters(std::tuple<T...> parameters) {
    int i = 0;
    std::apply(
        [&](auto &&...parameters) {
          (register_one(std::move(parameters), i++, type_name<T>), ...);
        },
        std::move(parameters));
  }

  template <typename... T>
  requires(sizeof...(T) >= 2) void with_parameters(auto... e) {
    with_parameters(std::tuple{std::move(e)...});
  }

  template <typename T>
  void with_parameters(std::initializer_list<T> parameters) {
    with_parameter_range(parameters);
  }
};

namespace expect_helper {

export struct Begin {};
export struct End {
  std::string_view condition_string;
};

export struct Expectation {
  char const *file;
  int line;
  std::string failure;

  explicit operator bool() const { return failure.empty(); }

  Expectation &&operator or(auto on_fail) && {
    if (*this) {
      on_fail(std::back_inserter(failure));
    }
    return std::move(*this);
  }

  ~Expectation() {
    if (*this) return;
    GTEST_MESSAGE_AT_(file, line, failure.c_str(),
                      ::testing::TestPartResult::kNonFatalFailure);
  }
};
enum { EQ, NE, GT, GE, LT, LE };

export template <typename C>
struct Condition {
  C const &condition;
};
export template <typename C>
Condition<C> operator<=(Begin, C const &condition) {
  return {condition};
}
export template <typename C>
std::string operator<<=(Condition<C> c, End e) {
  if (c.condition) return {};

  std::string s;
  s += "Expected: ";
  int negation = e.condition_string.starts_with("not ") ? 4
               : e.condition_string.starts_with("!")    ? 1
                                                        : 0;
  s += e.condition_string.substr(negation);
  if constexpr (not std::is_same_v<std::decay_t<decltype(c.condition)>, bool>) {
    s += " (";
    s += testing::PrintToString(c.condition);
    s += ")";
  }
  s += "\n    to be ";
  s += (negation ? "false" : "true");
  return s;
}

export template <typename L, auto OP, typename R>
struct Comparison {
  L const &lhs;
  R const &rhs;

  constexpr bool check() const {
    if constexpr (OP == EQ) return lhs == rhs;
    if constexpr (OP == NE) return lhs != rhs;
    if constexpr (OP == GT) return lhs > rhs;
    if constexpr (OP == GE) return lhs >= rhs;
    if constexpr (OP == LT) return lhs < rhs;
    if constexpr (OP == LE) return lhs <= rhs;
  }

  static constexpr std::string_view name = [] {
    if constexpr (OP == EQ) return "==";
    if constexpr (OP == NE) return "!=";
    if constexpr (OP == GT) return ">";
    if constexpr (OP == GE) return ">=";
    if constexpr (OP == LT) return "<";
    if constexpr (OP == LE) return "<=";
  }();
};
export template <typename L, typename R>
Comparison<L, EQ, R> operator==(Condition<L> lhs, R const &rhs) {
  return {lhs.condition, rhs};
}
export template <typename L, typename R>
Comparison<L, NE, R> operator!=(Condition<L> lhs, R const &rhs) {
  return {lhs.condition, rhs};
}
export template <typename L, typename R>
Comparison<L, GT, R> operator>(Condition<L> lhs, R const &rhs) {
  return {lhs.condition, rhs};
}
export template <typename L, typename R>
Comparison<L, GE, R> operator>=(Condition<L> lhs, R const &rhs) {
  return {lhs.condition, rhs};
}
export template <typename L, typename R>
Comparison<L, LT, R> operator<(Condition<L> lhs, R const &rhs) {
  return {lhs.condition, rhs};
}
export template <typename L, typename R>
Comparison<L, LE, R> operator<=(Condition<L> lhs, R const &rhs) {
  return {lhs.condition, rhs};
}
export template <typename L, auto OP, typename R>
std::string operator<<=(Comparison<L, OP, R> c, End e) {
  if (c.check()) return {};

  auto lhs = testing::PrintToString(c.lhs);
  auto i = e.condition_string.find(c.name);
  //__________
  //www == www
  //w == w 
  //
  //www == www
  //..w == w 
  //__________
  //w == w 
  //www == www
  //
  //..w == w 
  //www == www
  //__________
  //wwwwwwwwww
  //w == w 
  //wwwww == w
  //
  //wwwwwwwwww
  //w ....== w 
  //wwwww == w
  int offset = 0;
  if (i != std::string_view::npos) {
    offset = int(i) - lhs.size() - 1;
  }
  std::string s;
  s += "Expected: ";
  if (offset < 0) {
    s += std::string(-offset, ' ');
  }
  s += e.condition_string;
  s += "\n";
  s += "  Actual: ";
  if (offset > 0) {
    s += std::string(offset, ' ');
  }
  s += testing::PrintToString(c.lhs);
  s += " vs ";
  s += testing::PrintToString(c.rhs);
  return s;
}

template <typename... C>
struct MultiEquality {
  std::tuple<C const &...> tuple;
  bool equal;
};
export template <typename L, typename R, typename T>
MultiEquality<L, R, T> operator==(Comparison<L, EQ, R> c, T const &rhs) {
  return {
      {c.lhs, c.rhs, rhs},
      c.check() and c.rhs == rhs
  };
}
export template <typename... C, typename R>
MultiEquality<C..., R> operator==(MultiEquality<C...> c, R const &rhs) {
  bool equal = c.equal and std::get<sizeof...(C) - 1>(c.tuple) == rhs;
  return {std::tuple_cat(c.tuple, std::tie(rhs)), equal};
}
export template <typename... C>
std::string operator<<=(MultiEquality<C...> c, End e) {
  if (c.equal) return {};

  std::string s;
  std::apply(
      [&](auto const &c0, auto const &...c) {
        s += "Expected: ";
        s += e.condition_string;
        s += "\n";
        s += "  Actual: ";
        s += testing::PrintToString(c0);
        s += (... + (" vs " + testing::PrintToString(c)));
      },
      c.tuple);
  return s;
}

template <typename... C>
struct Ordering {
  std::tuple<C const &...> tuple;
  bool ordered;
};
export template <typename L, auto OP, typename R, typename T>
requires(OP == LE or OP == LT) Ordering<L, R, T> operator<(Comparison<L, OP, R> c,
                                                           T const &rhs) {
  return {
      {c.lhs, c.rhs, rhs},
      c.check() and c.rhs < rhs
  };
}
export template <typename L, auto OP, typename R, typename T>
requires(OP == LE or OP == LT) Ordering<L, R, T> operator<=(Comparison<L, OP, R> c,
                                                            T const &rhs) {
  return {
      {c.lhs, c.rhs, rhs},
      c.check() and c.rhs <= rhs
  };
}
export template <typename... C, typename R>
Ordering<C..., R> operator<(Ordering<C...> c, R const &rhs) {
  bool ordered = c.ordered and std::get<sizeof...(C) - 1>(c.tuple) < rhs;
  return {std::tuple_cat(c.tuple, std::tie(rhs)), ordered};
}
export template <typename... C, typename R>
Ordering<C..., R> operator<=(Ordering<C...> c, R const &rhs) {
  bool ordered = c.ordered and std::get<sizeof...(C) - 1>(c.tuple) <= rhs;
  return {std::tuple_cat(c.tuple, std::tie(rhs)), ordered};
}
export template <typename... C>
std::string operator<<=(Ordering<C...> c, End e) {
  if (c.ordered) return {};

  std::string s;
  std::apply(
      [&](auto const &c0, auto const &...c) {
        s += "Expected: ";
        s += e.condition_string;
        s += "\n";
        s += "  Actual: ";
        s += testing::PrintToString(c0);
        s += (... + (" vs " + testing::PrintToString(c)));
      },
      c.tuple);
  return s;
}

// TODO: Matchers. Add in an expectation with <<=
//   EXPECT_(a <<= InRange(0, 10) and not Eq(9));
// Declare with a lambda
//   auto IsEven = matcher([](auto i) { return i % 2 == 0; })
//     .describe("is even")
//     .describe_negation("is not even");

}  // namespace expect_helper

export template <typename T>
struct Generator {
  struct promise_type {
    std::optional<T> value;
    std::exception_ptr exception;

    Generator get_return_object() {
      return {std::coroutine_handle<promise_type>::from_promise(*this)};
    }
    std::suspend_always initial_suspend() { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void unhandled_exception() { exception = std::current_exception(); }

    template <std::convertible_to<T> From>
    std::suspend_always yield_value(From &&from) {
      value = std::forward<From>(from);
      return {};
    }
    void return_void() {}
  };

  struct sentinel {};

  struct iterator {
    std::coroutine_handle<promise_type> &handle;

    bool operator==(sentinel) const { return handle.done(); }

    iterator &operator++() {
      handle.resume();
      if (handle.promise().exception) {
        // Note that if we deferred this to operator* we might skip an exception
        // if we were only iterating and not reading the range.
        std::rethrow_exception(std::move(handle.promise().exception));
      }
      return *this;
    }

    T operator*() const {
      // Since this is an input range, we're not required to guarantee the
      // validity of `*it` independent of `*it++`.
      T value = std::move(*handle.promise().value);
      handle.promise().value.reset();
      return value;
    }
  };

  sentinel end() { return {}; }
  iterator begin() { return ++iterator{handle}; }

  std::coroutine_handle<promise_type> handle;
  ~Generator() { handle.destroy(); }
};
