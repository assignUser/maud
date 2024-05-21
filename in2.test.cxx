module;
#include <array>
#include <coroutine>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string_view>
module test_;

import maud_;
import yml_;

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
  static auto tree = parse_in_place(cases.data());
  return tree.rootref();
}();

TEST_(compilation, CASES) {
  std::string name{strv(parameter["name"])};
  if (not parameter.has_child("compiled")) return;

  auto in2 = strv(parameter["template"]);
  auto expected_compiled = strv(parameter["compiled"]);

  EXPECT_(compile_in2(std::string(in2)) >>= HasSubstr(expected_compiled));
  std::ofstream{TEMP / (name + ".e.in2.cmake"s)} << "include(Maud)\n"
                                                 << expected_compiled;
}

TEST_(rendering, CASES) {
  std::string name{strv(parameter["name"])};

  auto in2 = strv(parameter["template"]);

  auto compiled_path = TEMP / (name + ".in2.cmake"s);
  std::ofstream{compiled_path} << "include(Maud)\n" << compile_in2(std::string(in2));

  auto rendered_path = TEMP / name;
  std::ofstream{rendered_path} << "";

  auto cmd = "cmake"s;
  cmd += " -DRENDER_FILE=\"" + rendered_path.string() + "\"";
  cmd += " -DCMAKE_MODULE_PATH=\"" + (DIR / "cmake_modules").string() + "\"";
  cmd += " -P \"" + compiled_path.string() + "\"";

  if (parameter.has_child("rendered")) {
    if (not EXPECT_(std::system(cmd.c_str()) == 0)) return;
    EXPECT_(read(rendered_path) == strv(parameter["rendered"]));
  }

  if (parameter.has_child("render error")) {
    cmd += " 2> \"" + rendered_path.string() + "\"";
    EXPECT_(std::system(cmd.c_str()) != 0);
    EXPECT_(read(rendered_path) >>= ContainsRegex(strv(parameter["render error"])));
  }
}
