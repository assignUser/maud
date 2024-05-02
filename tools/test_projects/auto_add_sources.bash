source ../../test_project.bash

cat >foo.cxx <<-EOF
	export module foo;
	  export int foo() { return 0; }
EOF

maud --log-level=VERBOSE

cat >bar.cxx <<-EOF
	export module bar;
	  export int bar() { return 0; }
EOF

cmake --build build --config Debug
[ -e build/Debug/libbar.a ]
