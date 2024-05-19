module;
#include <algorithm>
#include <coroutine>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string_view>

#define RYML_SINGLE_HDR_DEFINE_NOW
#include "ryml.hxx"
module test_;

import maud_;

using std::operator""s;

std::string read(std::filesystem::path const &path) {
  std::ifstream stream{path};
  std::string contents;
  contents.resize(stream.seekg(0, std::ios_base::end).tellg());
  stream.seekg(0).read(contents.data(), contents.size());
  return contents;
}

auto const DIR = std::filesystem::path{__FILE__}.parent_path();
auto const TEMP = std::filesystem::temp_directory_path();
auto const CASES = [] {
  static auto cases = read(DIR / "in2.test.yaml");
  static auto tree = ryml::parse_in_place(cases.data());
  return tree.rootref();
}();

std::string str(auto v) {
  return v.get() ? std::string{v.val().data(), v.val().size()} : "__"s;
}

struct In2Case {
  std::string name, in2, expected_rendered, expected_compiled;
  friend void PrintTo(In2Case c, std::ostream *os) { *os << c.name; }
};

TEST_(compilation, []() -> Generator<In2Case> {
  for (auto c : CASES) {
    if (not c["compiled"].get()) continue;
    co_yield In2Case{
        str(c["name"]),
        str(c["template"]),
        {},
        str(c["compiled"]),
    };
  }
}) {
  auto [name, in2, _, expected_compiled] = parameter;

  auto compiled = compile_in2(std::move(in2));
  for (auto *p : {&compiled, &expected_compiled}) {
    while (p->back() == '\n') {
      p->pop_back();
    }
    *p += "\n";
  }
  EXPECT_(compiled == expected_compiled);
  std::ofstream{TEMP / (name + ".e.in2.cmake")} << "include(Maud)\n" << expected_compiled;
}

TEST_(rendering, []() -> Generator<In2Case> {
  for (auto c : CASES) {
    co_yield In2Case{
        str(c["name"]),
        str(c["template"]),
        str(c["rendered"]),
        {},
    };
  }
}) {
  auto [name, in2, expected_rendered, _] = parameter;

  auto compiled_path = TEMP / (name + ".in2.cmake");
  std::ofstream{compiled_path} << "include(Maud)\n" << compile_in2(std::move(in2));

  auto rendered_path = TEMP / name;
  std::ofstream{rendered_path} << "";

  auto cmd = "cmake"s;
  cmd += " -DRENDER_FILE=\"" + rendered_path.string() + "\"";
  cmd += " -DCMAKE_MODULE_PATH=\"" + (DIR / "cmake_modules").string() + "\"";
  cmd += " -P \"" + compiled_path.string() + "\"";

  if (not EXPECT_(std::system(cmd.c_str()) == 0)) return;
  EXPECT_(read(rendered_path) == expected_rendered);
}
