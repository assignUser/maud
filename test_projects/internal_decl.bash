source ../../test_project.bash

>foo_primary.cxx cat<<-EOF
	export module foo;
  export import :part;
EOF

>foo_part.cxx cat<<-EOF
	export module foo:part;
	export int foo();
	export int foo_half();
	int foo_internal(); // not exported, but usable within module foo
EOF

>foo_impl.cxx cat<<-EOF
	module foo;
  import :part;
	int foo() { return foo_internal(); }
EOF

>foo_half_impl.cxx cat<<-EOF
	module foo:half_impl;
  import :part;
	int foo_half() { return foo_internal() / 2; }
EOF

>foo_internal_impl.cxx cat<<-EOF
	module foo;
	int foo_internal() { return 3; }
EOF

maud
