// Boost Licensed
//

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

import executable;
import maud_;

// TODO catch errors which are definitely a problem in the interface block:
// - unterminated string
// - malformed comment
// - more than a single module decl
// - `module` followed by not-a-module-name
// - `import` followed by not-a-module-name
// - `module` or `import` declaration not followed by attributes then semicolon
// - anything other than a PP directive in the global module fragment
// - malformed attributes

// FIXME skip attributes

// FIXME handle header units

// TODO replace <iostream> with <format>

// TODO replace char const* with Location and track lines for better error reporting
template <char... CHARS>
constexpr auto first_of = [](auto s) {
  while (*s != 0) {
    bool any_matched = (... or (*s == CHARS));
    if (any_matched) break;
    ++s;
  }
  return s;
};

template <char... CHARS>
constexpr auto first_not_of = [](auto s) {
  while (*s != 0) {
    bool any_matched = (... or (*s == CHARS));
    if (not any_matched) break;
    ++s;
  }
  return s;
};

void chomp_until(auto delimiter, auto &s) {
  // NOTE: if the delimiter doesn't find anything, we'll chomp out the whole string
  s = delimiter(s);
}

bool try_chomp_prefix(std::string_view prefix, auto &s) {
  auto i = s;
  for (char c : prefix) {
    if (c != *i++) return false;
  }
  s = i;
  return true;
}

void chomp_until_end_of_string_literal(auto &s) {
  if (*s == 0) return;
  assert(s[0] == '"');

  if (s[-1] == 'R') {
    ++s;
    auto tag_begin = s;
    chomp_until(first_of<'('>, s);
    auto tag_end = s;
    ++s;

    while (true) {
      chomp_until(first_of<')'>, s);

      if (*s == 0) [[unlikely]] {
        // There was no terminating "; badly formed C++ source
        // Just bail (chomping everything)
        return;
      }

      ++s;
      if (memcmp(tag_begin, s, tag_end - tag_begin) == 0) {
        s += tag_end - tag_begin;
        if (s[0] == '"') [[likely]] {
          ++s;
          return;
        }
      }
    }
  }

  while (true) {
    ++s;
    chomp_until(first_of<'"', '\\'>, s);

    if (*s == 0) [[unlikely]] {
      // There was no terminating "; badly formed C++ source
      // Just bail (chomping everything)
      return;
    }

    if (s[0] == '"') [[likely]] {
      break;
    }

    ++s;
  }
  assert(s[0] == '"');
  ++s;  // the closing quote needs to be chomped
}

void chomp_past_unescaped_line_ending(auto &s) {
  while (true) {
    chomp_until(first_of<'"', '\n', '\r'>, s);

    if (s[0] == '"') {
      chomp_until_end_of_string_literal(s);
      continue;
    }

    if (*s == 0) [[unlikely]] {
      break;
    }

    bool escaped = s[-1] == '\\';

    s += s[0] == '\r' and s[1] == '\n'  // check for CRLF line ending
           ? 2
           : 1;

    if (not escaped) break;
  }
}

void chomp_past_whitespace(auto &s) {
  chomp_until(first_not_of<' ', '\n', '\r', '\t'>, s);
}

std::string chomp_name(auto &s) {
  auto name_begin = s;
  chomp_until(
      [](auto s) {
        constexpr auto ID = [](char c) {
          if (c >= 'a' and c <= 'z') return true;
          if (c >= 'A' and c <= 'Z') return true;
          if (c >= '0' and c <= '9') return true;
          if (c == '_' or c == '.') return true;
          return false;
        };
        while (*s != 0) {
          if (not ID(*s)) break;
          ++s;
        }
        return s;
      },
      s);
  return {name_begin, s};
}

void scan_file(std::string_view path, auto s) {
  bool saw_export = false;

  bool is_interface = false;
  bool is_partition = false;
  std::string logical_name, maud_module_name;
  std::vector<std::string> requires_logical_names;

  while (*s != 0) {
    chomp_past_whitespace(s);
    switch (s[0]) {
      case ';':
        ++s;
        continue;

      case '#':
        chomp_past_unescaped_line_ending(s);
        continue;

      case '/':
        if (s[1] == '/') {
          chomp_past_unescaped_line_ending(s);
          continue;
        }

        if (s[1] == '*') {
          s += 2;
          while (true) {
            chomp_until(first_of<'*'>, s);
            if (s[1] == '/') break;
            ++s;
          }
          continue;
        }

        // nothing else could have started with '/'
        goto done;

      case 'm':
        if (try_chomp_prefix("module", s)) {
          chomp_past_whitespace(s);
          if (*s == ';') {
            // global module fragment; nothing to extract
            ++s;
            continue;
          }

          // module implementation unit
          auto name = chomp_name(s);

          // is it a partition?
          chomp_until(first_of<';', ':'>, s);
          if (*s == ';') {
            // not a partition
            ++s;

            maud_module_name = name;
            if (saw_export) {
              saw_export = false;
              is_interface = true;
              logical_name = std::move(name);
            } else {
              requires_logical_names.push_back(std::move(name));
            }
            continue;
          }

          // partition
          is_partition = true;
          ++s;
          chomp_past_whitespace(s);
          logical_name = name + ":" + chomp_name(s);
          maud_module_name = std::move(name);

          if (saw_export) {
            saw_export = false;
            is_interface = true;
          }

          chomp_until(first_of<';'>, s);
          ++s;
          continue;
        }

      case 'i':
        if (try_chomp_prefix("import", s)) {
          if (saw_export) {
            saw_export = false;
          }
          chomp_past_whitespace(s);
          if (*s == ':') {
            ++s;
            chomp_past_whitespace(s);
            requires_logical_names.push_back(maud_module_name + ":" + chomp_name(s));
          } else {
            requires_logical_names.push_back(chomp_name(s));
          }
          chomp_until(first_of<';'>, s);
          ++s;
          continue;
        }

        // nothing else could have started with 'i'
        goto done;

      case 'e':
        if (try_chomp_prefix("export", s)) {
          saw_export = true;
          continue;
        }

        // nothing else could have started with 'e'
        goto done;

      default:
        goto done;
    }
  }

done:
  std::cout << "{\n";
  std::cout << "  \"revision\": 0,\n";
  std::cout << "  \"rules\": [\n";
  std::cout << "    {\n";
  std::cout << "      \"primary-output\": \"" << path << ".o\"\n";

  if (is_partition or is_interface) {
    std::cout << "      \"provides\": [\n";
    std::cout << "        {\n";
    std::cout << "          \"is-interface\": " << (is_interface ? "true" : "false");
    std::cout << ",\n";
    std::cout << "          \"logical-name\": \"" << logical_name << "\"";
    std::cout << ",\n";
    std::cout << "          \"source-path\": \"" << path << "\"";
    std::cout << "\n";
    std::cout << "        }\n";
    std::cout << "      ]";

    if (not requires_logical_names.empty()) {
      std::cout << ",";
    }
    std::cout << "\n";
  }

  if (not requires_logical_names.empty()) {
    bool first = true;
    std::cout << "      \"requires\": [\n";
    for (auto const &name : requires_logical_names) {
      if (not first) {
        std::cout << ",\n";
      }
      std::cout << "        {\n";
      std::cout << "          \"logical-name\": \"" << name << "\"\n";
      std::cout << "        }";
      first = false;
    }
    std::cout << "\n";
    std::cout << "      ]\n";
  }

  std::cout << "    }\n";
  std::cout << "  ],\n";
  std::cout << "  \"version\": 1\n";
  std::cout << "}\n";
}

void test_chomp_until_end_of_string_literal(char const *cases) {
  while (*cases != 0) {
    if (cases[0] == '#') {
      chomp_until(first_of<'\n'>, cases);
      ++cases;
      continue;
    }

    auto *in_begin = cases;
    chomp_until(first_of<';'>, cases);
    std::string in{in_begin, cases};
    in.resize(in.size() + 8);
    ++cases;

    auto *expected_begin = cases;
    chomp_until(first_of<'\n'>, cases);
    std::string expected{expected_begin, cases};
    ++cases;

    auto *data = in.c_str();
    chomp_until(first_of<'"'>, data);
    chomp_until_end_of_string_literal(data);
    std::string content{in.c_str(), data};
    if (content != expected) {
      std::cout << "------------------------------"   //
                << "\nfor      `" << in << "`"        //
                << "\nexpected `" << expected << "`"  //
                << "\nbut got  `" << content << "`" << std::endl;
    }
  }
}

int main(int, char **) {
  char const *files = std::getenv("FILES_TO_SCAN");
  if (files) {
    while (*files != 0) {
      auto *c_file = files;
      chomp_until(first_of<';'>, files);
      std::string_view file{c_file, files};
      if (*files != 0) {
        ++files;
      }

      if (file.empty()) continue;
      // TODO single-headerify and then vendor boost interprocess so that
      // we can use a mapped file. We usually won't need the whole file in
      // memory to read the interface block; just the first few pages should do.
      auto contents = read(file);
      scan_file(file, contents.c_str());
    }
  }

  auto cases = read("end_of_string_literal.cases");
  test_chomp_until_end_of_string_literal(cases.c_str());

  return 0;
}
