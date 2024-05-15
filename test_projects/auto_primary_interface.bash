source ../../test_project.bash

>foo_a.cxx cat<<-EOF
	export module foo:a;
  export int a() { return 0; }
EOF

>foo_b.cxx cat<<-EOF
	export module foo:b;
  import :a;
  export int b() { return a() + 1; }
EOF

>use.cxx cat<<-EOF
	module executable;
	import foo;
  int main() { return a() + b(); }
EOF

maud --log-level=VERBOSE
