source ../../test_project.bash

mkdir fmt_42
cd fmt_42
>fmt_42.cxx cat<<-EOF
	module;
	#include <fmt/format.h>
	export module fmt_42;
  export std::string fmt_42() { return fmt::format("{}", 42); }
EOF
>fmt_42.cmake cat<<-EOF
	find_package(fmt REQUIRED)
	add_library(fmt_42)
	target_link_libraries(fmt_42 INTERFACE fmt::fmt)
	
	get_target_property(d fmt::fmt INTERFACE_INCLUDE_DIRECTORIES)
	set(options " \${CMAKE_INCLUDE_SYSTEM_FLAG_CXX} \${d} ")

	set_source_files_properties(
	  "\${dir}/fmt_42.cxx"
	  PROPERTIES
	  MAUD_PREPROCESSING_SCAN_OPTIONS "\${options}"
	)
EOF
maud --log-level=VERBOSE
cmake --install build --prefix $TEST_DIR/usr --config Debug
cd ..

mkdir use
cd use
>use.cxx cat<<-EOF
	module executable;
  import fmt_42;
  int main() { return fmt_42().size(); }
EOF
export CMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH:$TEST_DIR/usr/lib/cmake
maud --log-level=VERBOSE

# This fails because the installed export doesn't include a
# call to find_package(fmt):
#
# CMake Error at /home/ben/maud/tools/build/_maud/test_projects/usr/lib/cmake/fmt_42.maud-config.cmake:60 (set_target_properties):
#  The link interface of target "fmt_42" contains:
#
#    fmt::fmt
