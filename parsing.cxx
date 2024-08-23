// Boost Licensed
//
module;
#include <cstdint>
#include <string>
export module maud_:parsing;

template <bool INVERT, char... CHARS>
struct CharPredicate {
  constexpr bool operator()(char c) const {
    bool match = ((c == CHARS) or ...);
    return INVERT ? not match : match;
  }

  constexpr auto operator not() const { return CharPredicate<not INVERT, CHARS...>{}; }
};

export template <char... L, char... R>
constexpr auto operator or(CharPredicate<false, L...> l, CharPredicate<false, R...> r) {
  return CharPredicate<false, L..., R...>{};
}

export template <char... CHARS>
constexpr CharPredicate<false, CHARS...> OF{};

export constexpr auto SPACE = OF<' ', '\r', '\n', '\t'>;

export constexpr auto find_first(auto predicate, auto str) {
  while (*str != 0 and not predicate(*str)) {
    ++str;
  }
  return str;
}

export struct Location {
  Location(char const *begin) : line_begin{begin} {}
  char const *line_begin;
  uint16_t line = 0, column = 0;

  char const &operator*() const { return line_begin[column]; }

  Location operator++(int) {
    Location copy = *this;
    ++*this;
    return copy;
  }

  Location &operator++() {
    if (line_begin[column++] == '\n') {
      line_begin += column;
      column = 0;
      ++line;
    }
    return *this;
  }

  std::string_view view_line() const {
    return {line_begin, find_first(OF<'\r', '\n'>, &**this)};
  }

  std::string_view view_to(Location end) const { return {&**this, &*end}; }

  std::string line_column() const {
    return std::to_string(line + 1) + ":" + std::to_string(column + 1);
  }

  bool operator==(Location const &) const = default;
};
