// Boost Licensed
//
module;
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
export module maud_:in2;

using std::operator""s;

export void compile_in2(std::istream &is, std::ostream &os);

std::ostream *os = &std::cout;

template <bool INVERT, char... CHARS>
struct CharPredicate {
  constexpr bool operator()(char c) const {
    bool match = ((c == CHARS) or ...);
    return INVERT ? not match : match;
  }

  constexpr auto operator not() const { return CharPredicate<not INVERT, CHARS...>{}; }
};

template <char... L, char... R>
constexpr auto operator or(CharPredicate<false, L...> l, CharPredicate<false, R...> r) {
  return CharPredicate<false, L..., R...>{};
}

template <char... CHARS>
constexpr CharPredicate<false, CHARS...> OF{};

constexpr auto SPACE = OF<' ', '\r', '\n', '\t'>;

constexpr auto find_first(auto predicate, auto str) {
  while (*str != 0 and not predicate(*str)) {
    ++str;
  }
  return str;
}

struct Location {
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

auto find_end_of_quoted_string(auto str) {
  assert(*str == '"');
  while (true) {
    ++str;
    str = find_first(OF<'"', '\\'>, str);
    if (*str == 0) return str;
    if (*str == '"') return ++str;
    assert(*str == '\\');
    ++str;
  }
}

auto find_end_of_block_string(auto str) {
  assert(*str == '[');
  auto begin = ++str;
  str = find_first(not OF<'='>, str);
  if (*str != '[') return str;

  size_t open_len = &*str - &*begin;
  while (true) {
    str = find_first(OF<']'>, str);
    if (*str == 0) return str;

    begin = ++str;
    str = find_first(not OF<'='>, str);
    if (*str == 0) return str;
    if (*str != ']') continue;

    size_t close_len = &*str - &*begin;
    if (close_len != open_len) continue;

    return ++str;
  }
}

auto find_end_of_string(auto str) {
  return *str == '"' ? find_end_of_quoted_string(str) : find_end_of_block_string(str);
}

auto literal(auto begin) {
  auto end = begin;
  std::string bracket_fill;

  while (true) {
    end = find_first(OF<
                         // A literal chunk is always ended by @.
                         '@',
                         // A closing bracket followed by zero or more
                         // equals might require expanding the string's
                         // bracket.
                         ']'>,
                     end);

    if (*end != ']') break;

    auto bracket = end++;
    end = find_first(not OF<'='>, end);
    if (*end != ']' and *end != '@') continue;

    // `Hello ]=] ` ->
    // `render([=[Hello ]=] ]=])`
    //                  ^^^ oops not the end I want.
    //
    // `Hello ]=@` ->
    // `render([=[Hello ]=]=])`
    //                  ^^^ oops not the end I want.
    //
    // `Hello ]=  ` ->
    // `render([=[Hello ]=  ]=])`
    //                  ^^ it's fine that's not an end.
    size_t len = &*end - &*bracket;
    if (bracket_fill.size() >= len) continue;

    bracket_fill.resize(len, '=');
  }

  // Don't bother rendering an empty literal.
  if (begin == end) return end;

  debug("literal", begin, end);

  *os << "render([" << bracket_fill << "[" << (*begin == '\n' ? "\n" : "")
      << begin.view_to(end) << ']' << bracket_fill << "])\n";
  return end;
}

auto skipping_strings_find_first(auto real_end, auto str) {
  while (true) {
    str = find_first(real_end or OF<'[', '"'>, str);
    if (real_end(*str) or *str == 0) return str;

    // A string might contain characters matching real_end,
    // so skip past the string to make sure we don't return
    // prematurely.
    str = find_end_of_string(str);
    if (*str == 0) return str;
  }
}

auto pipeline(auto begin, auto end) {
  debug("pipeline init", begin, end);
  *os << "set(IT ";
  reference(begin);
  *os << ")\n";

  int depth = 0;
  while (true) {
    ++end;
    begin = find_first(not SPACE, end);
    // Find the end of the pipeline or the next filter
    end = skipping_strings_find_first(OF<'@', '|'>, begin);

    if (std::string_view v{&*begin, &*end}; v == "foreach") {
      debug("pipeline foreach", begin, end);
      *os << "set(foreach_IT_" << depth << ")\nforeach(IT ${IT})\n";
      ++depth;
    } else if (v == "endforeach") {
      debug("pipeline endforeach", begin, end);
      *os << "list(APPEND foreach_IT_" << depth << " \"${IT}\")\nendforeach()\n"
          << "set(IT \"${foreach_IT_" << depth << "}\")\n";
      --depth;
    } else {
      debug("pipeline filter", begin, end);
      *os << "template_filter_" << begin.view_to(end) << "\n";
    }

    if (*end != '|') break;
  }
  // TODO assert depth == 0

  debug("pipeline output", end, end);
  *os << "render(\"${IT}\")\n";
  if (*end == 0) return end;
  return ++end;
}

void reference(auto begin) {
  begin = find_first(not SPACE, begin);
  auto end = find_first(SPACE or OF<'@'>, begin);
  *os << "\"${" << begin.view_to(end) << "}\"";
}

auto const HASH_LINE = std::string(82, '#') + "\n";

void debug(auto type, Location begin, Location end) {
  *os << "\n# " << type << " " << begin.line_column() << "-" << end.line_column() << "\n"
      << HASH_LINE  //
      << "# " << begin.view_line() << "\n";

  if (begin.line == end.line) {
    *os << "# " << std::string(begin.column, ' ');
    if (int len = end.column - begin.column; len >= 2) {
      *os << '^' << std::string(len - 2, '~');
    }
    *os << "^\n" << HASH_LINE;
    return;
  }

  *os << "# " << std::string(begin.column, ' ') << '^'
      << std::string(begin.view_line().size() - begin.column, '~') << "\n#"
      << std::string(end.column, '~') << "v\n# " << end.view_line() << "\n"
      << HASH_LINE;
}

// str is assumed to be null terminated
void compile(auto begin) {
  if (*begin == 0) return;

  // we always start with a literal chunk
LITERAL:
  auto end = literal(begin);
  if (*end == 0) return;

  // skip past the @
  begin = ++end;

  // check for @@, in which case we resume with a
  // new literal
  if (*begin == '@') {
    debug("@@ -> @", begin, begin);
    *os << "render(\"@\")\n";
    ++begin;
    if (*begin == 0) return;
    goto LITERAL;
  }

  // Now we have a var ref, a command block, or
  // a pipe.
  end = find_first(OF<
                       // The block ends with @.
                       '@',
                       // A '(' indicates a command.
                       '(',
                       // A '|' indicates a pipeline.
                       '|'>,
                   begin);
  if (*end == '@' or *end == 0) {
    debug("reference", begin, end);
    *os << "render(";
    reference(begin);
    *os << ")\n";
    if (*end == 0) return;
    begin = ++end;
    goto LITERAL;
  }

  if (*end == '(') {
    end = skipping_strings_find_first(OF<'@'>, end);
    debug("commands", begin, end);
    *os << begin.view_to(end) << "\n";
    if (*end == 0) return;
    begin = ++end;
    goto LITERAL;
  }

  begin = pipeline(begin, end);
  goto LITERAL;
}

void compile_in2(std::istream &is, std::ostream &os) {
  std::string in2(std::istreambuf_iterator{is}, {});
  ::os = &os;
  compile(Location{in2.c_str()});
}
