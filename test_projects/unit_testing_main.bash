source ../../test_project.bash

>test_main.cxx cat <<-EOF
	module;
	#include <gtest/gtest.h>
	export module test_:main;

  export int foo;

	int main(int argc, char* argv[]) {
	  foo = 999;
	  testing::InitGoogleTest(&argc, argv);
	  return RUN_ALL_TESTS();
	}
EOF

>foo_check.cxx cat <<-EOF
	module test_;

	TEST_(check_foo_is_999) {
	  EXPECT_(foo == 999);
	}
EOF

>allow_preprocessing_scan.cmake cat<<-EOF
	#add_executable(test_.foo) # FIXME this isn't working as in use_find_package
	get_target_property(i GTest::gtest INTERFACE_INCLUDE_DIRECTORIES)
	include_directories(\${i})
EOF

maud --log-level=VERBOSE
$TEST_DIR/source/build/Debug/test_.foo_check
