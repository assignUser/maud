module;
#ifdef _WIN32
#include <Windows.h>
#else
#include <stdlib.h>
#endif
#include <filesystem>
#include <string>
export module maud_:environment;

export struct EnvironmentVariable {
  EnvironmentVariable(std::string name, auto mod) : name{std::move(name)} {
    if (char *var = std::getenv(this->name.c_str())) {
      original = var;
    }
    set(mod(original));
  }

  ~EnvironmentVariable() { set(original); }

  bool set(std::string const &value) {
#ifdef _WIN32
    return SetEnvironmentVariableA(name.c_str(), value.c_str());
#else
    return setenv(name.c_str(), value.c_str(), 1) == 0;
#endif
  }

  std::string name, original;
};

export struct WorkingDirectory {
  std::filesystem::path old = std::filesystem::current_path();

  explicit WorkingDirectory(std::filesystem::path const &wd) {
    std::filesystem::current_path(wd);
  }

  ~WorkingDirectory() { std::filesystem::current_path(old); }
};

#ifdef _WIN32
export std::string const PATH_VAR = "Path", PATH_SEP = ";";
#else
export std::string const PATH_VAR = "PATH", PATH_SEP = ":";
#endif
