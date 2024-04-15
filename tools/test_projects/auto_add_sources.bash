source ../../test_project.bash

>foo.cxx cat<<-EOF
	export module foo;
  export int foo() { return 0; }
EOF

maud --log-level=VERBOSE


>bar.cxx cat<<-EOF
	export module bar;
  export int bar() { return 0; }
EOF

ninja -C build
[ -e build/libbar.a ]
