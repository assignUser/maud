source ../../test_project.bash

cat >custom_test.cmake <<-EOF
	function(maud_add_test source_file partition out_target_name)
	  cmake_path(GET source_file STEM name)
	  set(\${out_target_name} "test_.\${name}" PARENT_SCOPE)
	
	  if(NOT TARGET "test_.\${name}")
	    add_executable(test_.\${name})
	  endif()
	  add_test(NAME test_.\${name} COMMAND $<TARGET_FILE:test_.\${name}>)
	  target_sources(
	    test_.\${name}
	    PRIVATE
	    FILE_SET module_providers
	    TYPE CXX_MODULES
      BASE_DIRS \${CMAKE_SOURCE_DIR}
	    FILES _test.cxx
	  )
	  set_target_properties(
	    test_.\${name}
	    PROPERTIES
	    MAUD_INTERFACE _test.cxx
	  )
  endfunction()
EOF

cat >_test.cxx <<-EOF
	module;
	#include <iostream>
	export module test_;
	export void expect(auto const &condition) { 
	  if (not condition) std::cerr << "failed: " << condition << std::endl;
	}
	export void expect_eq(auto const &l, auto const &r) { 
	  if (l != r) std::cerr << "failed: " << l << "==" << r << std::endl;
	}
EOF

cat >basics.cxx <<-EOF
	module test_;
	int main() {
	  expect_eq(1, 3);
	}
EOF

maud
ctest --test-dir build --output-on-failure -C Debug

# Assert that the test executable exists but isn't installed
cmake --install build --prefix $TEST_DIR/usr --config Debug
[   -e $TEST_DIR/source/build/Debug/test_.basics ]
[ ! -e $TEST_DIR/usr/bin/test_.basics ]
