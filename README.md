Maud
====

Maud simplifies building C++ projects by reducing configuration boilerplate.

```sh-session
$ ls
hello.cxx

$ cat hello.cxx
#include <iostream>

import executable;

int main() {
  std::cout << "hello world!" << std::endl;
}

$ maud --quiet

$ .build/Debug/hello
hello world!
```

Maud bootstraps a cmake build directory with excellent defaults and batteries
included. Maud makes building C++20 modules straightforward. Other
features include performant and expressive globbing, first class support for
generated files, inference of compilation/link/test targets from source files,
built-in targets for rendering gorgeous documentation, expanded capabilities
for declaring and resolving build options, and more.

Getting Started
---------------

Maud is itself a Maud-based project. Build with:

```sh-session
$ git clone https://github.com/bkietz/maud.git && cd maud

# get dependencies with flox
$ flox activate

$ maud --log-level=VERBOSE # Pass cmake -D options etc here

# optionally, install:
$ cmake --install .build --config Debug
```

Maud uses
[Ninja Multi-Config](https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html#ninja-generators)
by default, but recent versions of MSVC/Visual Studio also support C++20 modules.

If you don't already have the `maud` executable on your PATH, you can bootstrap using:

```shell-session
$ cmake -P cmake_modules/maud_cli.cmake -- --log-level=VERBOSE
# ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~^
#   (the maud executable is just an alias for this anyway)
```

(TODO link to rendered docs) To build the documentation, install the python dependencies
in cmake_modules/sphinx_requirements.txt and Doxygen. When the dependencies are detected,
rendering documentation will be added to the build step.
