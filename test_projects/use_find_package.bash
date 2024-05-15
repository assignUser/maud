source ../../test_project.bash

>use_json_fmt.cxx cat<<-EOF
	module;
	#include <fmt/format.h>
	#include <nlohmann/json.hpp>
	module executable;
	
	int main() {}
EOF

>use_json_fmt.cmake cat<<-EOF
	find_package(nlohmann_json REQUIRED)
	find_package(fmt REQUIRED)
	
	add_executable(use_json_fmt)
	target_link_libraries(
	  use_json_fmt
	  PRIVATE
	  nlohmann_json::nlohmann_json
	  fmt::fmt-header-only
	)
	
	set(options "-DFMT_HEADER_ONLY=1")
	foreach(target fmt::fmt-header-only nlohmann_json::nlohmann_json)
	  get_target_property(i \${target} INTERFACE_INCLUDE_DIRECTORIES)
	  foreach(d \${i})
	    string(APPEND options " \${CMAKE_INCLUDE_SYSTEM_FLAG_CXX} \${d}")
	  endforeach()
	endforeach()

	set_source_files_properties(
	  "\${dir}/use_json_fmt.cxx"
	  PROPERTIES
	  MAUD_PREPROCESSING_SCAN_OPTIONS "\${options}"
	)
EOF

maud
