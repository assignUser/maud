module;
#include <algorithm>
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

char const *find_substr(std::string_view haystack, std::string_view needle) {
  auto it = std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end());
  return it == haystack.end() ? nullptr : &*it;
}

void replace(std::string &haystack, std::string_view needle, fs::path replacement) {
  char const *n = find_substr(haystack, needle);
  if (not n) throw std::runtime_error("replace couldn't find the needle");
  haystack.replace(n - haystack.data(), needle.size(), replacement.string());
}

constexpr auto PATCH_TEMPLATE = R"(
  #### INJECTED BY MAUD ####
  include("<CacheVars.cmake>")
  include("<Maud.cmake>")
  _maud_maybe_regenerate()
  #### INJECTED BY MAUD ####
)";

fs::path build, maud_cmake;

int main(int argc, char **argv) try {
  if (argc != 3) {
    std::cerr << "maud_inject_regenerate <build> <Maud.cmake>" << std::endl;
    return EINVAL;
  }

  build = argv[1];
  maud_cmake = argv[2];

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

  std::string patch = PATCH_TEMPLATE;
  replace(patch, "<Maud.cmake>", maud_cmake);
  replace(patch, "<CacheVars.cmake>",
          build / "_maud" / "configure_cache_variables.cmake");

  if (find_substr(contents, patch)) return 0;

  auto mtime = fs::last_write_time(verify_globs);

  for (auto delay = 50ms; delay < 1000ms; delay *= 2) {
    if (std::ofstream stream{verify_globs, std::ios_base::app}) {
      stream << patch;
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
