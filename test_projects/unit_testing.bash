source ../../test_project.bash

cat >basics.cxx <<-EOF
	module;
	#include <coroutine>
	#include <string>
	module test_;

	using std::operator""s;

	SUITE_STATE { std::string yo = "yo"; };

	TEST_(DISABLED_empty) {}

	TEST_(basic) {
	  int three = 3, five = 5;
	  EXPECT_(three != five);
	  EXPECT_(three == three == 3);
	  EXPECT_(three < five <= 6);
	  EXPECT_(67 > five);

	  EXPECT_(three);
	  EXPECT_(not std::false_type{});

	  int a = 999, b = 88888;
	  EXPECT_(a != b);

	  int *ptr = &three;
	  if (not EXPECT_(ptr != nullptr)) return;
	  EXPECT_(*ptr == three);
	  EXPECT_(suite_state->yo == "yo");

	  EXPECT_("hello world"s >>= HasSubstr("llo"));
	}

	TEST_(parameterized, {111, 234}) {
	  EXPECT_(parameter == parameter);
	}

	TEST_(parameterized_gen, []() -> Generator<int> {
	  for (int i = 0; i < 10; ++i) {
	    co_yield i;
	  }
	}) {
	  EXPECT_(parameter < 10);
	}

	TEST_(typed, std::tuple{0, ""s}) {
	  EXPECT_(parameter + parameter == parameter);
	}
EOF

maud
ctest --test-dir build --output-on-failure -C Debug

# Assert that the test executable exists but isn't installed
cmake --install build --prefix $TEST_DIR/usr --config Debug
[   -e $TEST_DIR/source/build/Debug/test_.basics ]
[ ! -e $TEST_DIR/usr/bin/test_.basics ]
