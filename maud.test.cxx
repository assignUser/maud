module;
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

#include <stdlib.h>
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

std::ofstream write(std::filesystem::path const &path) {
  std::filesystem::create_directories(path.parent_path());
  return std::ofstream{path};
}

std::filesystem::path operator+(std::filesystem::path path, std::string_view ext) {
  path += ext;
  return path;
}

auto const DIR = std::filesystem::path{__FILE__}.parent_path();

auto const TEMP = [] {
  auto tmp = std::filesystem::temp_directory_path() / "maud";
  std::filesystem::create_directory(tmp);
  return tmp;
}();

auto const IN2_CASES = [] {
  static auto cases = read(DIR / "in2.test.yaml");
  static auto tree = parse_in_place(cases.data());
  return tree.rootref();
}();

TEST_(compilation, IN2_CASES) {
  auto name = to_string(parameter["name"]);
  if (not parameter.has_child("compiled")) return;

  auto in2 = to_view(parameter["template"]);
  auto expected_compiled = to_view(parameter["compiled"]);

  EXPECT_(compile_in2(std::string(in2)) >>= HasSubstr(expected_compiled));
  std::ofstream{TEMP / name + ".e.in2.cmake"s} << expected_compiled;
}

TEST_(rendering, IN2_CASES) {
  auto name = to_string(parameter["name"]);
  auto in2 = to_view(parameter["template"]);

  auto compiled_path = TEMP / name + ".in2.cmake"s;
  std::ofstream{compiled_path} << "include(Maud)\n"
                               << "include(MaudTemplateFilters)\n"
                               << compile_in2(std::string(in2));

  auto rendered_path = TEMP / name;
  std::ofstream{rendered_path} << "";

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

auto const PROJECT_CASES = [] {
  static auto cases = read(DIR / "project.test.yaml");
  static auto tree = parse_in_place(cases.data());
  return tree.rootref();
}();

TEST_(project, PROJECT_CASES) {
  auto name = to_string(parameter["name"]);

  std::filesystem::create_directories(TEMP / name);
  std::filesystem::remove_all(TEMP / name);

  auto src = TEMP / name / "source";
  std::filesystem::create_directories(src);

  auto usr = TEMP / name / "usr";
  std::filesystem::create_directories(usr);

  auto install_cmd = "cmake --install \""s + BUILD_DIR + "\" --prefix \""s + usr.string()
                   + "\" --config Debug";
  if (not EXPECT_(std::system(install_cmd.c_str()) == 0)) return;

  struct WorkingDir {
    std::filesystem::path old = std::filesystem::current_path();
    WorkingDir(std::filesystem::path const &wd) { std::filesystem::current_path(wd); }
    ~WorkingDir() { std::filesystem::current_path(old); }
  } _{src};

  return;

  for (auto command : parameter["commands"]) {
    if (not command.is_map()) {
      if (not EXPECT_(std::system(to_string(command).c_str()) == 0)) return;
      continue;
    }

    if (command.has_child("write")) {
      write(src / to_view(command["write"])) << to_view(command["contents"]);
      continue;
    }

    if (command.has_child("exists")) {
      EXPECT_(std::filesystem::exists(to_string(command["exists"])));
      continue;
    }

    if (command.has_child("does not exist")) {
      EXPECT_(not std::filesystem::exists(to_string(command["does not exist"])));
      continue;
    }
  }
}
