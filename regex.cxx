// Boost Licensed
//
module;

#include <array>
#include <regex>
#include <string_view>
#include <unordered_map>

export module regex;

export template <int N>
struct Result {
  bool success;
  std::array<std::string_view, N> groups;
  explicit operator bool() const { return success; }
};

/// Subclass of std::array constructible from array ref
export template <typename T, std::size_t N>
struct Array : std::array<T, N> {
  constexpr Array(T const (&arr)[N]) {
    for (T const *ptr = arr; T & c : *this) c = *ptr++;
  }
};

export template <Array L>
struct string_constant {};

export template <Array L>
consteval string_constant<L> operator""_() { return {}; }

std::regex const &regex(std::string const &);

struct RegexSource {
  explicit consteval RegexSource(std::string_view source) {
    auto chomp = [&](std::string_view prefix) {
      if (source.starts_with(prefix)) {
        stripped.append(prefix);
        source = source.substr(prefix.size());
        return true;
      }
      return false;
    };

    auto chomp1 = [&] {
      stripped.append(source.substr(0, 1));
      source = source.substr(1);
    };

    int depth = 0;
    bool in_class = false;

    while (not source.empty()) {
      if (chomp("[")) {
        in_class = true;
        continue;
      }

      if (chomp("]")) {
        in_class = false;
        continue;
      }

      if (in_class) {
        chomp1();
        continue;
      }

      if (chomp(" ") or chomp("\r") or chomp("\n")) {
        stripped.resize(stripped.size() - 1);
        continue;
      }

      if (chomp("#")) {
        stripped.resize(stripped.size() - 1);
        auto i = source.find_first_of('\n');
        if (i == std::string_view::npos) break;
        source = source.substr(i + 1);
        continue;
      }

      if (chomp("\\(") or chomp("\\)") or chomp("\\[") or chomp("\\]")) {
        continue;
      }

      if (chomp("(?:")) {
        ++depth;
        continue;
      }

      if (chomp("(")) {
        if (depth == 0) {
          ++group_count;
        }
        ++depth;
        continue;
      }

      if (chomp(")")) {
        --depth;
        continue;
      }

      chomp1();
    }
  }

  int group_count = 1;
  std::string stripped;
};

template <Array L>
constexpr RegexSource regex_source{{L.data(), L.size()}};

export template <Array L>
Result<regex_source<L>.group_count> operator/(std::string_view s, string_constant<L>);

// This causes an undefined symbol error
//module : private;

std::regex const &regex(std::string const &source) {
  std::unordered_map<char const *, std::regex> regexes;
  auto [it, success] = regexes.insert({source.c_str(), {}});
  if (success) {
    it->second = std::regex{source};
  }
  return it->second;
}

using SubMatch = std::sub_match<std::string_view::const_iterator>;

struct Allocator {
  using value_type = SubMatch;
  static auto constexpr alignment = alignof(SubMatch);

  template <typename U>
  struct rebind {
    static_assert(std::is_same_v<U, SubMatch>);
    using other = Allocator;
  };

  SubMatch *allocate(std::size_t) { return buffer; }
  void deallocate(SubMatch *, std::size_t) noexcept {}

  bool operator==(Allocator other) const { return true; }

  SubMatch *buffer;
};

template <Array L>
Result<regex_source<L>.group_count> operator/(std::string_view s, string_constant<L>) {
  constexpr int N = regex_source<L>.group_count;

  std::array<SubMatch, N> match_results_storage;
  std::match_results<std::string_view::const_iterator, Allocator> match_results{
      Allocator{match_results_storage.data()}};

  Result<N> result{
    .success = std::regex_search(s.begin(), s.end(), match_results, regex(regex_source<L>.stripped))
  };
  if (result.success){
    auto *group = result.groups.data();
    for (std::pair sub : match_results) {
      *group++ = {sub.first, sub.second};
    }
  }
  return result;
}
void use() {
  "" / ""_;
  ""/"(.*)"_;
  ""/R"(  (\(.*)  )"_;
  ""/R"(  (\(.*) [(] )"_;
}
