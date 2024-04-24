module;
#include <algorithm>
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

int main(int argc, char **argv) try {
  if (argc != 3) {
    throw std::runtime_error("maud_inject_regenerate <build> <Maud.cmake>");
  }

  fs::path build{argv[1]};
  fs::path maud_cmake{argv[2]};

  fs::path verify_globs = build / "CMakeFiles" / "VerifyGlobs.cmake";
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

  std::string patch = PATCH_TEMPLATE;
  replace(patch, "<Maud.cmake>", maud_cmake);
  replace(patch, "<CacheVars.cmake>",
          build / "_maud" / "configure_cache_variables.cmake");

  if (find_substr(contents, patch)) return 0;

  auto mtime = fs::last_write_time(verify_globs);
  std::ofstream{verify_globs, std::ios_base::app} << patch;
  fs::last_write_time(verify_globs, mtime);
} catch (std::exception const &e) {
  std::cerr << "maud_inject_regenerate error: " << e.what() << std::endl;
  return 1;
}
