module;
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
module test_;

import maud_;

using std::operator""s;

auto const TEST_DIR = std::filesystem::path{BUILD_DIR} / "_maud/scan_tests";

TEST_(scan) {
  auto yaml = R"(
    revision: 0
    rules:
    - primary-output: ""
      requires: []
  )"s;
  auto tree = parse_in_place(yaml.data());
  auto rule = tree["rules"][0];

  rule["primary-output"] = "maud_.cxx.o";
  set_map(rule["requires"], 0)["logical-name"] = "FOO";

  std::cout << as_json(tree) << std::endl;
}
