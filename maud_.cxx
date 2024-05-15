// Boost Licensed
//
module;
#include <iosfwd>
export module maud_;

/// compile a .in2 template to cmake
export void compile_in2(std::istream &is, std::ostream &os);
