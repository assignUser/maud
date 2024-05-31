// leave this in cmake_modules/; it makes bootstrapping easier
// since Maud.cmake assumes that _test_.* are next to it.
module;
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <any>
#include <coroutine>
#include <cstdint>
#include <exception>
#include <sstream>
#include <vector>
export module test_;
export import :main;

using namespace testing;

export template <typename T>
std::string const type_name = testing::internal::GetTypeName<T>();

export template <>
auto const type_name<std::string> = "std::string";

export template <>
auto const type_name<std::string_view> = "std::string_view";

export struct Main {
  Main(int &argc, char **argv) { InitGoogleTest(&argc, argv); }
  int run() { return RUN_ALL_TESTS(); }
};

template <typename T>
concept Complete = requires {
  { sizeof(T) } -> std::same_as<std::size_t>;
};

template <typename R>
concept SizedRange = requires(R range) {
  { range.size() } -> std::same_as<std::size_t>;
};

std::vector<std::any> parameters;

struct Info {
  char const *file;
  int line;
  char const *suite_name;
  char const *test_name;
};

using Body = void(void const *);

template <typename Test, typename Parameter>
static void body(void const *p) {
  Test::body(*static_cast<Parameter const *>(p));
}

export template <typename S>
struct Registrar {
  using SuiteState = std::conditional_t<Complete<S>, S, int>;

  static SuiteState *const suite_state;

  struct Fixture : testing::Test {
    static void SetUpTestSuite() { new (suite_state) SuiteState{}; }
    static void TearDownTestSuite() { suite_state->~SuiteState(); }

    Body *_body;
    void const *_parameter;
    void TestBody() override { _body(_parameter); }

    template <typename Test, typename Parameter>
    Fixture(Test *, Parameter const *p) : _body{&body<Test, Parameter>}, _parameter{p} {}
  };

  void register_one(auto *test, Info info, auto parameter, int i = -1,
                    std::string type_name = "") {
    constexpr bool HAS_PARAMETER =
        not std::is_same_v<decltype(parameter), std::nullptr_t>;

    auto [file, line, suite_name, test_name] = info;

    std::string name = test_name;
    if (i != -1) {
      name += "/" + PrintToString(i);
    }
    if (not type_name.empty()) {
      name += "/" + type_name;
    }
    if constexpr (HAS_PARAMETER) {
      name += "/" + PrintToString(*parameter);
    }
    testing::RegisterTest(suite_name, name.c_str(), nullptr, nullptr, file, line,
                          [test, parameter] {
                            if constexpr (HAS_PARAMETER) {
                              return new Fixture{test, parameter};
                            } else {
                              constexpr auto NULLPTR = nullptr;
                              return new Fixture{test, &NULLPTR};
                            }
                          });
  }

  void register_range(auto *test, Info info, auto &&range) {
    std::vector<std::decay_t<decltype(*range.begin())>> vector;
    if constexpr (SizedRange<decltype(range)>) {
      vector.reserve(range.size());
    }
    for (auto &&parameter : range) {
      vector.push_back(std::move(parameter));
    }
    for (int i = 0; auto const &parameter : vector) {
      register_one(test, info, &parameter, i++);
    }
    parameters.emplace_back(std::move(vector));
  }

  void register_(auto *test, Info info, auto &&parameters) {
    if constexpr (std::is_invocable_v<decltype(parameters)>) {
      register_range(test, info, std::move(parameters)());
    } else {
      register_range(test, info, std::move(parameters));
    }
  }

  template <typename T>
  void register_(auto *test, Info info, std::initializer_list<T> parameters) {
    register_range(test, info, parameters);
  }

  void register_(auto *test, Info info, auto &&...parameters)
    requires(sizeof...(parameters) != 1)
  {
    if constexpr (sizeof...(parameters) == 0) {
      register_one(test, info, nullptr);
    } else {
      register_(test, info, std::tuple{std::move(parameters)...});
    }
  }

  template <typename... T>
  void register_(auto *test, Info info, std::tuple<T...> tuple) {
    parameters.emplace_back(std::move(tuple));
    std::apply(
        [&, i = 0](auto const &...parameters) mutable {
          (register_one(test, info, &parameters, i++, type_name<T>), ...);
        },
        std::any_cast<decltype(tuple) const &>(parameters.back()));
  }
};

template <typename S>
typename Registrar<S>::SuiteState *const Registrar<S>::suite_state = [] {
  alignas(SuiteState) static char storage[sizeof(SuiteState)];
  return std::launder(reinterpret_cast<SuiteState *>(&storage));
}();

namespace expect_helper {

export struct Begin {};
export struct End {
  std::string_view condition_string;
};

export struct Expectation {
  char const *file;
  int line;
  std::string failure;

  operator bool() const { return failure.empty(); }

  Expectation &&operator or(auto on_fail) && {
    if (not failure.empty()) {
      std::stringstream ss{std::move(failure)};
      on_fail(ss);
      failure = std::move(ss).str();
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
std::string operator,(Condition<C> c, End e) {
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
std::string operator,(Comparison<L, OP, R> c, End e) {
  if (c.check()) return {};

  auto lhs = testing::PrintToString(c.lhs);
  auto i = e.condition_string.find(c.name);
  //__________
  // www == www
  // w == w
  //
  // www == www
  //..w == w
  //__________
  // w == w
  // www == www
  //
  //..w == w
  // www == www
  //__________
  // wwwwwwwwww
  // w == w
  // wwwww == w
  //
  // wwwwwwwwww
  // w ....== w
  // wwwww == w
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
std::string operator,(MultiEquality<C...> c, End e) {
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
  requires(OP == LE or OP == LT)
Ordering<L, R, T> operator<(Comparison<L, OP, R> c, T const &rhs) {
  return {
      {c.lhs, c.rhs, rhs},
      c.check() and c.rhs < rhs
  };
}
export template <typename L, auto OP, typename R, typename T>
  requires(OP == LE or OP == LT)
Ordering<L, R, T> operator<=(Comparison<L, OP, R> c, T const &rhs) {
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
std::string operator,(Ordering<C...> c, End e) {
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

template <typename C>
struct MatchCondition {
  C const &condition;
  testing::Matcher<C const &> matcher;
};
export template <typename C, typename M>
MatchCondition<C> operator>>=(Condition<C> c, M matcher) {
  return {c.condition, SafeMatcherCast<C const &>(std::move(matcher))};
}
export template <typename C>
std::string operator,(MatchCondition<C> c, End e) {
  auto &[condition, matcher] = c;
  auto cs = e.condition_string;
  cs = cs.substr(cs.find_first_not_of(" \n\t\r"));
  cs = cs.substr(0, cs.find(">>="));
  std::stringstream stream;
  stream << "  Expected: " << cs;
  ::testing::internal::StreamMatchResultListener listener{&stream};
  if (matcher.MatchAndExplain(condition, &listener)) return {};
  stream << " ";
  matcher.DescribeTo(&stream);
  stream << "\n  Argument was: " << PrintToString(condition);
  return std::move(stream).str();
}

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

export using testing::internal::AnythingMatcher;

export using testing::A;
export using testing::An;

export using testing::Eq;
export using testing::Ge;
export using testing::Gt;
export using testing::Le;
export using testing::Lt;
export using testing::Ne;
// export using testing::IsFalse;
// export using testing::IsTrue;
export using testing::IsNull;
export using testing::NotNull;
export using testing::Optional;
export using testing::VariantWith;
export using testing::Ref;
export using testing::TypedEq;

export using testing::DoubleEq;
export using testing::FloatEq;
export using testing::NanSensitiveDoubleEq;
export using testing::NanSensitiveFloatEq;
export using testing::IsNan;
export using testing::DoubleNear;
export using testing::FloatNear;
export using testing::NanSensitiveDoubleNear;
export using testing::NanSensitiveFloatNear;

export using testing::ContainsRegex;
export using testing::EndsWith;
export using testing::HasSubstr;
// export using testing::IsEmpty;
export using testing::MatchesRegex;
export using testing::StartsWith;
export using testing::StrCaseEq;
export using testing::StrCaseNe;
export using testing::StrEq;
export using testing::StrNe;
export using testing::WhenBase64Unescaped;

export using testing::BeginEndDistanceIs;
export using testing::ContainerEq;
export using testing::Contains;
export using testing::Each;
export using testing::ElementsAre;
export using testing::ElementsAreArray;
// export using testing::IsEmpty;
export using testing::IsSubsetOf;
export using testing::IsSupersetOf;
export using testing::Pointwise;
export using testing::SizeIs;
export using testing::UnorderedElementsAre;
export using testing::UnorderedElementsAreArray;
export using testing::UnorderedPointwise;
export using testing::WhenSorted;
export using testing::WhenSortedBy;

export using testing::Field;
export using testing::Key;
export using testing::Pair;
export using testing::FieldsAre;
export using testing::Property;

export using testing::ResultOf;
export using testing::AllArgs;
export using testing::Args;

export using testing::Address;
export using testing::Pointee;
export using testing::Pointer;
export using testing::WhenDynamicCastTo;

export using testing::AllOf;
export using testing::AllOfArray;
export using testing::AnyOf;
export using testing::AnyOfArray;
export using testing::Not;
export using testing::Conditional;

export template <typename Condition, typename Describe>
struct Matcher {
  Condition condition;
  Describe describe;

  using is_gtest_matcher = void;

  bool MatchAndExplain(auto const &arg, std::ostream *os) const {
    return condition(arg, *os);
  }

  void DescribeTo(std::ostream *os) const { describe(false, *os); }
  void DescribeNegationTo(std::ostream *os) const { describe(true, *os); }
};
