module;
#include <cerrno>
#include <chrono>
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
  #### BEGIN INJECTED BY MAUD ####
  set(MAUD_CODE "_maud_maybe_regenerate()")
  include("${CMAKE_CURRENT_LIST_DIR}/../_maud/eval.cmake")
  ##### END INJECTED BY MAUD #####
)cmake";

fs::path build;

int main(int argc, char **argv) try {
  if (argc != 2) {
    std::cerr << "USAGE ERROR: maud_inject_regenerate <BUILD>" << std::endl;
    return EINVAL;
  }

  build = argv[1];

  fs::path script = build / "CMakeFiles" / "VerifyGlobs.cmake";
  fs::path flag = build / "CMakeFiles" / "cmake.verify_globs";
  std::cout << "trying to inject into " << script << std::endl;

  std::this_thread::sleep_for(50ms);
  while (not fs::exists(script) or not fs::exists(flag)) {
    std::cout << "didn't exist yet" << std::endl;
    std::this_thread::sleep_for(50ms);
  }

  std::string contents;
  {
    std::ifstream stream{script};
    contents.resize(stream.seekg(0, std::ios_base::end).tellg());
    stream.seekg(0).read(contents.data(), contents.size());
  }

  for (auto delay = 50ms; delay < 1000ms; delay *= 2) {
    if (std::ofstream stream{script, std::ios_base::app}) {
      stream << PATCH;
      stream.close();
      // Touching the script has made it newer than build.ninja, which
      // will trigger regeneration even if nothing else has changed.
      // We can overwrite its mtime to prevent that. The flag is never
      // touched except to trigger regeneration, so we can reuse its
      // mtime as "not newer than debug.ninja".
      fs::last_write_time(script, fs::last_write_time(flag));
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
  (stream ? stream : std::cerr) << e.what() << std::endl;
  return 1;
}
