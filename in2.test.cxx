module;
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
module test_;

import maud_;

using std::operator""s;

auto const CASES = Parameter::read_file(DIR / "in2.test.yaml");
auto const TEST_DIR = std::filesystem::path{BUILD_DIR} / "_maud/in2_tests";

TEST_(compilation, CASES) {
  auto name = parameter.name();
  if (not parameter.has_child("compiled")) return;

  auto in2 = to_view(parameter["template"]);
  auto expected_compiled = to_view(parameter["compiled"]);

  EXPECT_(compile_in2(std::string(in2)) >>= HasSubstr(expected_compiled));
  write(TEST_DIR / name + ".e.in2.cmake"s) << expected_compiled;
}

TEST_(rendering, CASES) {
  auto name = parameter.name();
  auto in2 = to_view(parameter["template"]);

  auto compiled_path = TEST_DIR / name + ".in2.cmake"s;
  write(compiled_path) << "include(Maud)\n"
                       << compile_in2(std::string(in2));

  auto rendered_path = TEST_DIR / name;
  write(rendered_path) << "";

  auto cmd = "cmake"s;
  if (parameter.has_child("definitions")) {
    for (auto def : parameter["definitions"]) {
      cmd += " -D"s + to_string(def);
    }
  }
  cmd += " -DRENDER_FILE=\"" + rendered_path.string() + "\"";
  cmd += " -DCMAKE_MODULE_PATH=\"" + (DIR / "cmake_modules").string() + "\"";
  cmd += " -P \"" + compiled_path.string() + "\"";

  if (parameter.has_child("rendered")) {
    if (not EXPECT_(std::system(cmd.c_str()) == 0)) return;
    EXPECT_(read(rendered_path) == to_view(parameter["rendered"]));
  }

  if (parameter.has_child("render error")) {
    cmd += " 2> \"" + rendered_path.string() + "\"";
    EXPECT_(std::system(cmd.c_str()) != 0);
    EXPECT_(read(rendered_path) >>= ContainsRegex(to_view(parameter["render error"])));
  }
}
