module;
#include <ostream>
#define RYML_SINGLE_HDR_DEFINE_NOW
#include "c4/yml.hxx"
export module yml_;

namespace c4::yml {
export void PrintTo(ConstNodeRef n, std::ostream *os) { *os << n["name"].val(); }

export std::string to_string(ConstNodeRef n) { return {n.val().data(), n.val().size()}; }
export std::string_view to_view(ConstNodeRef n) { return {n.val().data(), n.val().size()}; }
}  // namespace c4::yml

export using c4::yml::parse_in_place;
