source ../../test_project.bash

mkdir foo
cd foo
>foo_.cxx cat<<-EOF
	export module foo_;
  export int foo_impl() { return 0; }
EOF
>foo.cxx cat<<-EOF
	export module foo;
  export int foo();
EOF
>foo_impl.cxx cat<<-EOF
	// FIXME we can't do this without a scanner that picks up impl units!
	module foo;
	import foo_;
  int foo() { return foo_impl(); }
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
