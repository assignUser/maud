module;
#include <filesystem>
#include <fstream>
#include <string>
export module maud_:filesystem;

export template <size_t N = 8>
class Padded {
 public:
  static constexpr size_t PADDING = N;

  explicit Padded(auto size) : _storage(static_cast<size_t>(size) + PADDING * 2, '\0') {}
  Padded() = default;

  size_t size() const { return _storage.size() - PADDING * 2; }
  char *data() { return _storage.data() + PADDING; }

  char const *c_str() const { return _storage.c_str() + PADDING; }
  operator std::string_view() const { return {c_str(), size()}; }

 private:
  std::string _storage;
};

// Read to Padded so we can always have plenty of zeros for look ahead and behind.
export template <size_t N = Padded<>::PADDING>
Padded<N> read(std::filesystem::path const &path) {
  std::ifstream stream{path};
  Padded<N> contents{stream.seekg(0, std::ios_base::end).tellg()};
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

export auto const DIR = std::filesystem::path{__FILE__}.parent_path();
