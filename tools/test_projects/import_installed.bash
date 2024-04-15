source ../../test_project.bash

mkdir foo
cd foo
>foo.cxx cat<<-EOF
	export module foo;
  export int foo() { return 0; }
EOF
maud --log-level=VERBOSE
cmake --install build --prefix $TEST_DIR/usr
cd ..

mkdir use
cd use
>use.cxx cat<<-EOF
	module executable;
  import foo;
  int main() { return foo(); }
EOF
export CMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH:$TEST_DIR/usr/lib/cmake
maud --log-level=VERBOSE
