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

template <typename F>
void exponential_backoff(F f) {
  auto delay = 50ms;
  while (true) {
    try {
      return f();
    } catch (std::exception const &e) {
      std::cout << " failed: '" << e.what() << "'\n";
    }
    if (delay > 1s) break;

    // retry with exponential back off
    std::cout << " retrying in " << (delay / 1ms) << "ms" << std::endl;
    std::this_thread::sleep_for(delay);
    delay *= 2;
  }
  std::cout << " gave up with retrying" << std::endl;
  throw std::runtime_error("timed out while retrying");
}

int main(int argc, char **argv) try {
  if (argc != 2) {
    std::cerr << "USAGE ERROR: maud_inject_regenerate <BUILD>" << std::endl;
    return EINVAL;
  }

  build = argv[1];

  fs::path script = build / "CMakeFiles" / "VerifyGlobs.cmake";
  fs::path flag = build / "CMakeFiles" / "cmake.verify_globs";
  fs::path patched = build / "_maud" / "VerifyGlobs.cmake.patched";
  std::cout << "trying to inject into " << script << std::endl;

  // Ideally, we could patch VerifyGlobs.cmake directly from CMakeLists.txt
  // (or use a built in cmake_language(RECONFIGURE_CHECK CODE ...) function).
  //
  // However CMakeLists.txt is finished and generation is finalized...
  //     https://github.com/Kitware/CMake/blob/159ba027b98813921b6b32227569f85f9611a05d/Source/cmake.cxx#L2561-L2564
  // ...before VerifyGlobs.cmake is ever written.
  //     https://github.com/Kitware/CMake/blob/159ba027b98813921b6b32227569f85f9611a05d/Source/cmake.cxx#L2618-L2619
  //
  // That being the case, the only way to patch VerifyGlobs.cmake is to launch
  // a background process which patches as soon as the file exists. (We need to
  // use `setsid` or `start /b` for this because without it execute_process()
  // waits for all child processes to complete.)

  std::this_thread::sleep_for(50ms);
  while (not fs::exists(script) or not fs::exists(flag)) {
    std::cout << "didn't exist yet, retrying in 50ms" << std::endl;
    std::this_thread::sleep_for(50ms);
  }

  // At this point, CMake has just finished (or will do so shortly).
  //
  // Frequently Ninja (or other build tool) will be running immediately after CMake,
  // and *its* first action is always to run the verification script to determine if
  // regeneration is necessary. It's acceptable if this first instance of Ninja uses
  // the unpatched verification script because it follows complete generation so
  // regeneration is definitely unnecessary (TODO more consistently skip
  // verification in this case).
  //
  // The worst case scenario is for Ninja to detect the change and trigger
  // regeneration right now, since this can lead to a chain of repeated spurious
  // regeneration.

  std::cout << "reading current script" << std::endl;
  std::string contents;
  exponential_backoff([&] {
    std::ifstream stream{script};
    contents.resize(stream.seekg(0, std::ios_base::end).tellg());
    stream.seekg(0).read(contents.data(), contents.size());
  });

  std::cout << "creating patched script" << std::endl;
  exponential_backoff([&] {
    std::ofstream{patched, std::ios_base::app} << contents << PATCH;
  });

  std::cout << "getting flag's mtime" << std::endl;
  fs::file_time_type mtime;
  exponential_backoff([&] {
    // Patching the script will make it newer than build.ninja, which will trigger
    // spurious regeneration. We can overwrite its mtime to prevent that. The flag is
    // never touched except to trigger regeneration, so we can reuse its mtime as "not
    // newer than debug.ninja".
    mtime = fs::last_write_time(flag);
    fs::last_write_time(patched, mtime);
  });

  std::cout << "swapping in patched script" << std::endl;
  exponential_backoff([&] {
    // A renamed separate patch is used to be more like an atomic operation (it is on some
    // platforms), then we set the mtime again just in case rename changed it.
    fs::rename(patched, script);
    fs::last_write_time(script, mtime);
  });
} catch (std::exception const &e) {
  std::ofstream stream{build / "_maud" / "maud_inject_regenerate.error"};
  (stream ? stream : std::cerr) << e.what() << std::endl;
  return 1;
}
