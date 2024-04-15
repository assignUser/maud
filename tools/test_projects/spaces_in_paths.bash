source ../../test_project.bash

mkdir -p "y tho"

>"y tho/foo bar.cxx" cat<<-EOF
	export module foo.bar;
EOF

>use.cxx cat<<-EOF
	module executable;
	import foo.bar;
  int main() {}
EOF

maud
