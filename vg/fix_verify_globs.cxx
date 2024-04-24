module;
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
module executable;

namespace fs = std::filesystem;

using std::operator""s;
using std::operator""ms;

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "fix_verify_globs </abs/to/VerifyGlobs.cmake> </abs/to/Maud.cmake>"
              << std::endl;
    return 1;
  }

  fs::path verify_globs{argv[1]};
  while (not fs::exists(verify_globs)) {
    std::this_thread::sleep_for(100ms);
  }
  std::this_thread::sleep_for(500ms);

  std::string contents;
  {
    std::ifstream stream{verify_globs};
    contents.resize(stream.seekg(0, std::ios_base::end).tellg());
    stream.seekg(0).read(contents.data(), contents.size());
  }

  std::string patch = R"(
    #### INJECTED BY MAUD ####
    include(")"s + argv[2] + R"(")
    _maud_maybe_regenerate()
    #### INJECTED BY MAUD ####
  )"s;

  auto it = std::search(contents.begin(), contents.end(), patch.begin(), patch.end());
  if (it != contents.end()) return 0;

  auto mtime = fs::last_write_time(verify_globs);
  std::ofstream{verify_globs, std::ios_base::app} << patch;
  fs::last_write_time(verify_globs, mtime);
}
