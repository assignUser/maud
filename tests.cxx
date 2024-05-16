module;
#include <algorithm>
#include <coroutine>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string_view>
module test_;

import maud_;

auto const DIR = std::filesystem::path{__FILE__}.parent_path();
auto const TEMP = std::filesystem::temp_directory_path();

struct In2Case {
  In2Case(auto begin, auto end) {
    static std::regex const CASE{
        R"(^NAME: (.*)\nTEMPLATE:\n([^]*)COMPILED:\n([^]*)RENDERED:\n([^]*)$)"};

    std::match_results<decltype(begin)> results;
    if (not std::regex_match(begin, end, results, CASE)) return;

    int i = 1;
    for (auto *field : {&name, &in2, &expected_compiled, &expected_rendered}) {
      *field = results[i++];
    }
  }
  std::string name, in2, expected_compiled, expected_rendered;
  friend void PrintTo(In2Case c, std::ostream *os) { *os << c.name; }
};

TEST_(in2, []() -> Generator<In2Case> {
  std::ifstream stream{DIR / "in2.test_cases"};
  std::string contents;
  contents.resize(stream.seekg(0, std::ios_base::end).tellg());
  stream.seekg(0).read(contents.data(), contents.size());
  std::string_view constexpr DELIMITER = "---\n";
  auto begin = contents.begin();
  while (true) {
    auto end = std::search(begin, contents.end(), DELIMITER.begin(), DELIMITER.end());
    if (std::string_view{&*begin, &*end}.starts_with("NAME:")) {
      co_yield In2Case{begin, end};
    }
    if (end == contents.end()) break;
    begin = end + DELIMITER.size();
  }
}) {
  if (not EXPECT_(parameter.name != "")) return;

  std::istringstream in{std::string(parameter.in2)};
  std::ostringstream compiled;
  compile_in2(in, compiled);
  EXPECT_(compiled.str() == parameter.expected_compiled);

  auto compiled_path = TEMP / (std::string{parameter.name} + ".in2.cmake");
  std::ofstream{compiled_path} << compiled.str();

  auto rendered_path = TEMP / (std::string{parameter.name});
  std::ofstream{rendered_path} << "";

  auto render_cmd = std::string{"cmake"}  //
                  + " -C \"" + (DIR / "cmake_modules" / "Maud.cmake").string() + "\""
                  + " -DCMAKE_MODULE_PATH=\"" + (DIR / "cmake_modules").string() + "\""
                  + " -DRENDER_FILE=\"" + rendered_path.string() + "\"" + " -P \""
                  + compiled_path.string() + "\"";

  if (not EXPECT_(std::system(render_cmd.c_str()) == 0)) return;

  std::ifstream stream{rendered_path};
  std::string contents;
  contents.resize(stream.seekg(0, std::ios_base::end).tellg());
  stream.seekg(0).read(contents.data(), contents.size());
  EXPECT_(contents == parameter.expected_rendered);
}
