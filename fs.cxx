module;
#include <filesystem>
#include <fstream>
#include <string>
export module maud_:fs;

export std::string read(std::filesystem::path const &path) {
  std::ifstream stream{path};
  std::string contents;
  contents.resize(stream.seekg(0, std::ios_base::end).tellg());
  stream.seekg(0).read(contents.data(), contents.size());
  return contents;
}

export std::ofstream write(std::filesystem::path const &path) {
  std::filesystem::create_directories(path.parent_path());
  return std::ofstream{path};
}

export std::filesystem::path operator+(std::filesystem::path path, std::string_view ext) {
  path += ext;
  return path;
}
