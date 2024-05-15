source ../../test_project.bash

>render_foo.cmake cat<<-EOF
	file(
	  WRITE "\${MAUD_DIR}/rendered/foo.cxx"
	  [[
	  export module foo;
	  import bar;
	  static_assert(BOOL);
	  static_assert(sizeof(INTS) == sizeof(int) * 11);
	  ]]
	)
EOF

>bar.cxx.in2 cat<<-EOF
	@include(MaudTemplateFilters)@
	export module bar;
	export int INTS[] = {@
	foreach(i RANGE 10)
	  render("\${i},")
	endforeach()
	@};
	export constexpr bool BOOL = @MAUD_DIR | if_else(1 0)@;
EOF

maud
[ -e build/Debug/libfoo.a ]
[ -e build/Debug/libbar.a ]
