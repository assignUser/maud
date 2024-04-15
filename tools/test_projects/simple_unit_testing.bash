source ../../test_project.bash

mkdir -p basics_test
>basics_test/basics.cxx cat<<-EOF
	module;
	#include <coroutine>
	#include <string>
	module test_;

	using std::operator""s;

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
    if (not EXPECT_(ptr != nullptr)) {
      return;
    }
    EXPECT_(*ptr == three);
	}

	TEST_(parameterized, {111, 234}) {
	  EXPECT_(parameter == parameter);
	}

  TEST_(parameterized_gen, [i = 0]() mutable -> Generator<int> {
    while (i < 10) {
      co_yield i++;
    }
  }) {
	  EXPECT_(parameter < 10);
	}

	TEST_(lifted_typed, std::tuple{0, ""s}) {
	  EXPECT_(parameter + parameter == parameter);
	}
EOF

>basics_test/labels cat<<-EOF
	basic
	simple
	okay
EOF

maud
ctest --test-dir build --output-on-failure
