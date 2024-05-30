module;
#include <filesystem>
#include <ostream>
#include <vector>
#define RYML_SINGLE_HDR_DEFINE_NOW
#include "c4/yml.hxx"
export module maud_:yml;
import :filesystem;

namespace c4::yml {
export std::string to_string(ConstNodeRef n) { return {n.val().data(), n.val().size()}; }
export std::string_view to_view(ConstNodeRef n) {
  return {n.val().data(), n.val().size()};
}
export void PrintTo(ConstNodeRef p, std::ostream *os) { *os << p; }
}  // namespace c4::yml

export using c4::yml::ConstNodeRef;

export using c4::yml::operator<<;
export using c4::yml::as_json;
export using c4::yml::parse_in_place;

export decltype(auto) set_map(c4::yml::NodeRef node, auto key) {
  node[key] |= c4::yml::MAP;
  return node[key];
}

export struct Parameter : c4::yml::ConstNodeRef {
  std::string name() const { return {key().data(), key().size()}; }

  friend void PrintTo(Parameter p, std::ostream *os) { *os << p.name(); }

  static auto read_file(std::filesystem::path const &path) {
    struct : std::vector<Parameter> {
      Padded<> yaml;
      c4::yml::Tree tree;
      c4::yml::ConstNodeRef rootref;
    } set;

    set.yaml = read(path);
    set.tree = parse_in_place(set.yaml.data());
    set.rootref = set.tree.rootref();
    set.resize(set.rootref.num_children());

    for (auto parameter = set.begin(); auto n : set.rootref) {
      *parameter++ = {n};
    }
    return set;
  }
};
