source ../../test_project.bash

>render_foo.cmake cat<<-EOF
	file(WRITE "\${MAUD_DIR}/rendered/foo.cxx" [[
	  export module foo;
	]])
EOF

maud --log-level=VERBOSE
[ -e build/Debug/libfoo.a ]
