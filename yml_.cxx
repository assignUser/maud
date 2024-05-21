module;
#include <ostream>
#define RYML_SINGLE_HDR_DEFINE_NOW
#include "c4/yml.hxx"
export module yml_;

namespace c4::yml {
export void PrintTo(ConstNodeRef n, std::ostream *os) { *os << n["name"].val(); }

export std::string_view strv(ConstNodeRef n) { return {n.val().data(), n.val().size()}; }
}  // namespace c4::yml

export using c4::yml::parse_in_place;
