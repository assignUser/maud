source ../../test_project.bash

>foo_primary.cxx cat<<-EOF
	export module foo;
	export int foo();
	int foo_internal(); // not exported, but usable within module foo
EOF

>foo_impl.cxx cat<<-EOF
	module foo;
	int foo() { return foo_internal(); }
EOF

>foo_internal_impl.cxx cat<<-EOF
	module foo;
	int foo_internal() { return 3; }
EOF

maud
