module;
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
module executable;

namespace fs = std::filesystem;

using std::operator""s;
using std::operator""ms;

constexpr std::string_view PATCH = R"cmake(
  #### INJECTED BY MAUD ####
  set(MAUD_CODE "_maud_maybe_regenerate()")
  include("${CMAKE_CURRENT_LIST_DIR}/../_maud/eval.cmake")
  #### INJECTED BY MAUD ####
)cmake";

fs::path build;

int main(int argc, char **argv) try {
  if (argc != 2) {
    std::cerr << "maud_inject_regenerate <BUILD>" << std::endl;
    return EINVAL;
  }

  build = argv[1];

  fs::path verify_globs = build / "CMakeFiles" / "VerifyGlobs.cmake";
  while (not fs::exists(verify_globs)) {
    std::this_thread::sleep_for(50ms);
  }

  std::string contents;
  {
    std::ifstream stream{verify_globs};
    contents.resize(stream.seekg(0, std::ios_base::end).tellg());
    stream.seekg(0).read(contents.data(), contents.size());
  }

  if (contents.find(PATCH) != std::string_view::npos) return 0;

  auto mtime = fs::last_write_time(verify_globs);

  for (auto delay = 50ms; delay < 1000ms; delay *= 2) {
    if (std::ofstream stream{verify_globs, std::ios_base::app}) {
      stream << PATCH;
      fs::last_write_time(verify_globs, mtime);
      std::cout << "success" << std::endl;
      return 0;
    }
    // retry writing to VerifyGlobs.cmake with exponential back off
    std::cout << "retrying in " << (delay / 1ms) << "ms" << std::endl;
    std::this_thread::sleep_for(delay);
  }
  throw std::runtime_error("timed out retrying to patch");
} catch (std::exception const &e) {
  std::ofstream stream{build / "_maud" / "maud_inject_regenerate.error"};
  (stream ? stream : std::cerr) << "error: " << e.what() << std::endl;
  return 1;
}
