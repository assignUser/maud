module;
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
module test_;

import maud_;

using std::operator""s;

auto const CASES = Parameter::read_file(DIR / "project.test.yaml");
auto const TEST_DIR = std::filesystem::path{BUILD_DIR} / "_maud/project_tests";

TEST_(project, CASES) {
  auto name = parameter.name();

  std::filesystem::create_directories(TEST_DIR);
  std::filesystem::remove_all(TEST_DIR);

  auto src = TEST_DIR / name;
  std::filesystem::create_directories(src);

  auto usr = TEST_DIR / "usr";
  std::filesystem::create_directories(usr);

  auto install_cmd = "cmake --install \""s + BUILD_DIR + "\" --prefix \""s + usr.string()
                   + "\" --config Debug";
  if (not EXPECT_(std::system(install_cmd.c_str()) == 0)) return;

  EnvironmentVariable _env[] = {
      {PATH_VAR,
       [&](std::string const &path) {
         return (usr / "bin").string() + PATH_SEP + path;  //
       }                           },
      {"CMAKE_PREFIX_PATH", [&](std::string const &path) {
         return (usr / "lib/cmake").string() + PATH_SEP + path;  //
       }}
  };

  for (auto command : parameter) {
    WorkingDirectory _wd{command.is_map() and command.has_child("working directory")
                             ? src / to_string(command["working directory"])
                             : src};

    if (not command.is_map()) {
      if (not EXPECT_(std::system(to_string(command).c_str()) == 0)) return;
      continue;
    }

    if (command.has_child("command")) {
      if (not EXPECT_(std::system(to_string(command["command"]).c_str()) == 0)) return;
      continue;
    }

    if (command.has_child("failing command")) {
      EXPECT_(std::system(to_string(command["failing command"]).c_str()) != 0);
      continue;
    }

    if (command.has_child("write")) {
      write(src / to_view(command["write"])) << to_view(command["contents"]);
      continue;
    }

    auto get = [](auto node, auto key) {
      if (node.is_map()) {
        return node[key.val()];
      }
      int i;
      key >> i;
      return node[i];
    };

    if (command.has_child("json")) {
      auto contents = read(src / to_view(command["json"]));
      auto actual_tree = parse_in_place(contents.data());
      auto actual_node = actual_tree.rootref();

      ConstNodeRef last;
      for (auto segment : command["expect"]["path"]) {
        actual_node = get(actual_node, segment);
        last = segment;
      }

      std::stringstream ss;
      ss << as_json(actual_node);
      auto actual = std::move(ss).str();

      ss = {};
      ss << as_json(get(command["expect"]["like"], last));
      auto expected = std::move(ss).str();

      EXPECT_(actual == expected);
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
