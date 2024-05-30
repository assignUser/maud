module;
#include <ostream>
#include <vector>
#define RYML_SINGLE_HDR_DEFINE_NOW
#include "c4/yml.hxx"
export module maud_:yml;

namespace c4::yml {
export std::string to_string(ConstNodeRef n) { return {n.val().data(), n.val().size()}; }
export std::string_view to_view(ConstNodeRef n) {
  return {n.val().data(), n.val().size()};
}
}  // namespace c4::yml

export using c4::yml::ConstNodeRef;

export using c4::yml::parse_in_place;

export using c4::yml::operator<<;
export using c4::yml::as_json;

export using c4::yml::MAP;
export using c4::yml::SEQ;

export struct Parameter : c4::yml::ConstNodeRef {
  std::string name() const { return {key().data(), key().size()}; }
  friend void PrintTo(Parameter p, std::ostream *os) { *os << p.name(); }

  struct Set : std::vector<Parameter> {
    std::string yaml;
    c4::yml::Tree tree;
    c4::yml::ConstNodeRef rootref;
  };

  static auto wrap(std::string yaml) {
    Set set;
    set.yaml = std::move(yaml);
    set.tree = parse_in_place(set.yaml.data());
    set.rootref = set.tree.rootref();
    set.resize(set.rootref.num_children());
    for (auto parameter = set.begin(); auto n : set.rootref) {
      *parameter++ = {n};
    }
    return set;
  }
};
