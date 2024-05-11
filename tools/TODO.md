NEXT
----

- more test projects
  - remove the dependency on bash
  - options:
    - old call signature still works
    - detect cycles
    - various validation
    - presets get written
  - assert unit tests are not installed
  - use a maud based project with fetchcontent
  - verify maud works while using c++23
- harden and test the scanner
- include the template compiler
- include the cmake generator

TODO: stages
------------

The maud sequence is:
- init
- cmake_modules
- in2
- include
- cxx_sources
- module_scan
- targets
- docs
... but what if we need to insert something somewhere other than
`cmake_modules`? For example: cpp2.in2 - the cpp2 glob is run in 
`cmake_modules`, before in2 renders, so rendered cpp2 are not scanned.

One solution is to make the stages explicitly configurable:

```cmake
maud_stage(cpp2_sources BEFORE module_scan)
maud_defer_until(cpp2_sources)
```

This would additionally mean we can just wait till targets for find_package:

```cmake
maud_defer_until(targets)
find_package(nlohmann_json REQUIRED)
find_package(fmt REQUIRED)
target_link_libraries(
  use_json_fmt
  PRIVATE
  nlohmann_json::nlohmann_json
  fmt::fmt-header-only
)
```

TODO: test suites
-----------------

I don't like directory-per-test; too prescriptive. Instead, although
we link all files in a dir to a single executable we should ensure
there's one *test* per suite==implementation unit of test_. (We
use `--gtest_filter` to select suites within that executable.)
What's the test executable named, though? `dir-dir-stem`.

TODO: cmake compendium
----------------------

What things are built into cmake that don't see enough use?

- include_guard()
- variable_watch()
- multi-line strings
- glob()
- list(TRANSFORM)
- `option(BUILD_TESTING)` is automatic, don't add an option for that!
- `option(BUILD_SHARED_LIBS)` too
- use a gui to view options; it's better


TODO: install BMIs?
-------------------

What happens when: cmake installs/packages a header-only library with a
dependency on another header-only library? It seems the imported targets
must use `find_package()` to specify that anything which links to them
must also link to the dependency. TODO try this in docker and see what
happens

I would really prefer to install BMIs, but this requires consumers to use
the same compiler (which if we're packaging... they might not even have).
I think we need to install module interface sources along with enough
cmake to compile them with whatever compiler is present...
This is certainly what automatic cmake exporting does; we are forbidden
*not* to install those sources.

Note somewhere that just like header files, module interfaces cannot
depend on language flags or other things which break the ABI

Alternatively: install BMIs to `$prefix/lib/bmi/$CompilerID/$module.*`.
If you try to use a module with a mismatched compiler there's an error.
We provide a reserved target called `interfaces`, so if you switch
compilers you can clone the project and build/install just BMIs.
Nice enough for demos where everything is a wrapper around FetchContent,
and we leave a TODO saying that we need a better way to cache these.

TODO: doc
---------

- Switch to .rst
- detect sphinx, doxygen, breathe, doc2dash, ...
- glob up .rst files and assemble a docset
- auto generate tables of contents for each directory

TODO: in2 addenda
-----------------

No literal at the end of a template file is fine, we use that for
templates which are using something else to write RENDER_FILE, like cpuinfo.hxx.in2:

    @# Write cpuinfo to a header
    execute_process(
      COMMAND cpuinfo
      OUTPUT_VARIABLE cpuinfo
    )
    render("auto constexpr CPUINFO = R\"(${cpuinfo})\";")

Also, you can run arbitrary commands in a pipeline filter:

```
@STUFF | if_else(0 1) | ()
message(STATUS "DEBUGGING ${IT}")@
```

`list(TRANSFORM)` might be useful enough to allow an explicit feature for sub-pipelines:

```
@SOME_LIST_OF_BOOL |foreach| if_else(ENABLED DISABLED) |endforeach| join(",")@
```

TODO: options
-------------

Render options.rst with all the options summarized.

TODO:
-----

Optional features are modules (and maybe partitions) which can be disabled.
We want to enable some features then get a hard verbose error if their
dependencies aren't available.

Since we generate installed cmake, warn if `CMAKE_CXX_FLAGS` and other
ambiguous options are set.

Good example modules: sse4, fmt, nlohmann_json, std stand-ins

Mauds and ROCRs haha... runtime option configuration resource is a GROFF
formatted help page which is used to generate CLI/env accessors and a
.rst doc file.

```
# set compile_options for scanned sources to point to maud's DDI
https://cmake.org/cmake/help/latest/prop_sf/COMPILE_OPTIONS.html
# then set cmake's scanning to OFF
https://cmake.org/cmake/help/latest/prop_sf/CXX_SCAN_FOR_MODULES.html
```

Notes
-----

add_custom_target's OUTPUT file carries a dependency automatically

validation of dependencies is left to c++; if the result is just an error
that sends the user back to the dependency provider then a `static_assert`
is just as good as whatever cmake might throw up

What is `IMPORTED_CXX_MODULES_COMPILE_DEFINITIONS`? Is this a shortcut to
the way I want to associate options?

Translation Units other than module interface units are not necessarily reachable:
https://timsong-cpp.github.io/cppwp/n4861/module.reach#1
Therefore: do not import anything which is not an interface unit! Doing so is
implementation defined.

There are only module interface units and module implementation units
https://timsong-cpp.github.io/cppwp/n4861/module.unit#2

Notes: dep scan
---------------

target.dir/source.cxx.o.ddi contains the names of exports and imports
`ninja -t commands | grep scan-deps` shows the command to generate it
https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p1689r5.html
CMake vars of note:
- `CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS` clang-scan-deps program
- `CMAKE_CXX_SCANDEP_SOURCE` compiler-indepenent command template for scan deps
  - https://github.com/Kitware/CMake/blob/v3.29.0/Modules/Compiler/GNU-CXX.cmake#L77
  - https://github.com/Kitware/CMake/blob/v3.29.0/Modules/Compiler/Clang-CXX.cmake#L48
  - https://github.com/Kitware/CMake/blob/v3.29.0/Modules/Compiler/MSVC-CXX.cmake#L82
- ... but I don't see this template being used anywhere? seems it should be in GetScanRule()
  https://github.com/Kitware/CMake/blob/v3.29.0/Source/cmNinjaTargetGenerator.cxx#L606
- Looks like it's a hidden sub-rule in every compile rule; no non-internal way to trigger it
  https://github.com/Kitware/CMake/blob/v3.29.0/Source/cmNinjaTargetGenerator.cxx#L678
- ... but we don't want to define targets then get a module graph, we want to
  derive targets from the module graph. Therefore we need to do the scan manually

#### Module implementation units of primary modules

`clang-scan-deps` does *not* populate a provider for these.
This is extremely annoying since I want to attach those implementation units
to the target for the module. TL;DR: this seems to be intended and correct
behavior, so this is another thing the custom scanner will need to do differently
(probably by storing `rule::_maud_implementation_unit_of`).

```c++
// foo-interface.cxx
export module foo;
// -> provides foo with is-interface=true
// -> { ... "rules": [ { ... "provides": [ { ... "is-interface": true, "logical-name": "foo" } ] } ] }

// foo-implementation.cxx
module foo;
// -> requires foo???
// -> { ... "rules": [ { ... "requires": [ { "logical-name": "foo" } ] } ] }

// ... but module implementation units for partitions *are* providers
// foo-part-implementation.cxx
module foo:bar;
// -> provides foo:bar with is-interface=false, no requires
// -> { ... "rules": [ { ... "provides": [ { ... "is-interface": false, "logical-name": "foo:bar" } ] } ] }
```

#### Why? How?

Clang's module collection is handled by the [preprocessor](
https://github.com/llvm/llvm-project/blob/llvmorg-17.0.6/clang/include/clang/Lex/Preprocessor.h#L2377-L2383)
and then by other classes which query it like [ModuleDepCollectorPP](
https://github.com/llvm/llvm-project/blob/llvmorg-17.0.6/clang/lib/Tooling/DependencyScanning/ModuleDepCollector.cpp#L346)

The preprocessor's ModuleDeclState picks up the [module name and type](
https://github.com/llvm/llvm-project/blob/llvmorg-17.0.6/clang/unittests/Lex/ModuleDeclStateTest.cpp#L145-L148)
and we are *in a named module*. (This test is unchanged in main, so it doesn't seem to have been a
bug to correct.) According to the unit test, a module partition is [neither impl nor interface unit](
https://github.com/llvm/llvm-project/blob/llvmorg-17.0.6/clang/unittests/Lex/ModuleDeclStateTest.cpp#L164-L167)
see also the [isImplementationUnit() accessor](
https://github.com/llvm/llvm-project/blob/llvmorg-17.0.6/clang/include/clang/Lex/Preprocessor.h#L570)
That *is* a bug or at least a mis-naming because it's not an interface unit *therefore* it is an
implementation unit.

ModuleDepCollectorPP is not unit tested unfortunately.
It produces P1689ModuleInfo in the [EndOfMainFile callback](
https://github.com/llvm/llvm-project/blob/llvmorg-17.0.6/clang/lib/Tooling/DependencyScanning/ModuleDepCollector.cpp#L377) and... here's a [comment describing this annoyance](
https://github.com/llvm/llvm-project/blob/llvmorg-17.0.6/clang/lib/Tooling/DependencyScanning/ModuleDepCollector.cpp#L382-L384)
(but I'm still wondering *why*):
```c++
    // Don't put implementation (non partition) unit as Provide.
    // Put the module as required instead. Since the implementation
    // unit will import the primary module implicitly.
```

Aha: [`CXX(20:module.unit#3)`](https://timsong-cpp.github.io/cppwp/n4868/module.unit#3)
"A named module shall not contain multiple module partitions with the same module-partition."
Therefore even module partition implementation units must be unique, which explains the 
different treatment.

#### What do GCC/MSVC do?

MSVC also treats an implementation unit as a requirer rather than a provider,
at least when the generator is ninja. Visual Studio generators don't seem
to use p1689:

```json
# foo-interface.cxx.ifc.d.json
{
    "Version": "1.2",
    "Data": {
        "Source": "c:\\users\\ben\\source\\repos\\modulestest\\modulestest\\foo-interface.cxx",
        "ProvidedModule": "foo",
        "Includes": [],
        "ImportedModules": [],
        "ImportedHeaderUnits": []
    }
}
```

These .ifc files are generated for foo-interface.cxx, but nothing is generated for non-interfaces.
These are referenced later when compiling (but not when linking) CL.command.1.tlog:

```
^C:\USERS\BEN\SOURCE\REPOS\MODULESTEST\MODULESTEST\FOO-INTERFACE.CXX
/c /ZI /nologo /W1 /WX- /diagnostics:column /Od /Ob0 /D _MBCS /D WIN32 /D _WINDOWS /D "CMAKE_INTDIR=\"Debug\"" /Gm- /EHsc /RTC1 /MDd /GS /fp:precise /Zc:wchar_t /Zc:forScope /Zc:inline /std:c++20 /ifcOutput "C:\USERS\BEN\SOURCE\REPOS\MODULESTEST\OUT\VS-BUILD\MODULESTEST\MODULESTEST.DIR\DEBUG\FOO-INTERFACE.CXX.IFC" /Fo"MODULESTEST.DIR\DEBUG\\" /Fd"MODULESTEST.DIR\DEBUG\VC143.PDB" /sourceDependencies "C:\USERS\BEN\SOURCE\REPOS\MODULESTEST\OUT\VS-BUILD\MODULESTEST\MODULESTEST.DIR\DEBUG\FOO-INTERFACE.CXX.IFC.D.JSON" /external:W1 /Gd /interface  /TP C:\USERS\BEN\SOURCE\REPOS\MODULESTEST\MODULESTEST\FOO-INTERFACE.CXX

^C:\USERS\BEN\SOURCE\REPOS\MODULESTEST\MODULESTEST\MAIN.CXX
/c /reference "FOO=C:\USERS\BEN\SOURCE\REPOS\MODULESTEST\OUT\VS-BUILD\MODULESTEST\MODULESTEST.DIR\DEBUG\FOO-INTERFACE.CXX.IFC" /ZI /nologo /W1 /WX- /diagnostics:column /Od /Ob0 /D _MBCS /D WIN32 /D _WINDOWS /D "CMAKE_INTDIR=\"Debug\"" /Gm- /EHsc /RTC1 /MDd /GS /fp:precise /Zc:wchar_t /Zc:forScope /Zc:inline /std:c++20 /Fo"MODULESTEST.DIR\DEBUG\\" /Fd"MODULESTEST.DIR\DEBUG\VC143.PDB" /external:W1 /Gd /TP C:\USERS\BEN\SOURCE\REPOS\MODULESTEST\MODULESTEST\MAIN.CXX

^C:\USERS\BEN\SOURCE\REPOS\MODULESTEST\MODULESTEST\FOO.CXX
/c /reference "FOO=C:\USERS\BEN\SOURCE\REPOS\MODULESTEST\OUT\VS-BUILD\MODULESTEST\MODULESTEST.DIR\DEBUG\FOO-INTERFACE.CXX.IFC" /ZI /nologo /W1 /WX- /diagnostics:column /Od /Ob0 /D _MBCS /D WIN32 /D _WINDOWS /D "CMAKE_INTDIR=\"Debug\"" /Gm- /EHsc /RTC1 /MDd /GS /fp:precise /Zc:wchar_t /Zc:forScope /Zc:inline /std:c++20 /Fo"MODULESTEST.DIR\DEBUG\\" /Fd"MODULESTEST.DIR\DEBUG\VC143.PDB" /external:W1 /Gd /TP C:\USERS\BEN\SOURCE\REPOS\MODULESTEST\MODULESTEST\FOO.CXX
```

Compare to the command used during ninja gen:
```
C:\PROGRA~1\MICROS~1\2022\COMMUN~1\VC\Tools\MSVC\1439~1.335\bin\Hostx64\x64\cl.exe   /DWIN32 /D_WINDOWS /W3 /GR /EHsc /MDd /Ob0 /Od /RTC1 -std:c++20 -MDd -ZI C:\Users\ben\source\repos\ModulesTest\ModulesTest\main.cxx -nologo -TP -showIncludes -scanDependencies ModulesTest\CMakeFiles\ModulesTest.dir\main.cxx.obj.ddi -FoModulesTest\CMakeFiles\ModulesTest.dir\main.cxx.obj
```

... anyways, it seems I can invoke the p1689 output whenever I need it


# Notes: template file output

If we allow a pipeline to start with a command,
we can put simple blocks on a single line.
`@foreach(i RANGE 8) | render("${i}. \n") | endforeach()@`
NOPE this requires checking for "namespaced" filters
which requires a variadic macro and cmake can't do those
... unless we have special syntax to mark prefixed calls
`@some_list |> join(",") | message("debug: ${IT}") |> strip()@`.
I think the complexity is not worth it.

TODO: generator expressions
---------------------------

nlohmann_json's COMPILE_OPTIONS property produces this horror:

```
$<
  $<NOT:$<BOOL:ON>>:JSON_USE_GLOBAL_UDLS=0>;
  $<$<NOT:$<BOOL:ON>>:JSON_USE_IMPLICIT_CONVERSIONS=0>;
  $<$<BOOL:OFF>:JSON_DISABLE_ENUM_SERIALIZATION=1>;
  $<$<BOOL:OFF>:JSON_DIAGNOSTICS=1>;
  $<$<BOOL:OFF>:JSON_USE_LEGACY_DISCARDED_VALUE_COMPARISON=1
>
```
It really ought to be possible to expand *most* generator
expressions at configure time. Someday it'd be amusing to write
a library to just do that.


AAARGH: globs
-------------

TODO: document the new solution somewhere, probably in maud_inject.cxx

Currently the hack is to have a single target named _maud_maybe_regenerate
on which all other targets depend. If globs/scan results differ, we
use cmake --reconfigure-during-build. However that terminates the current
build command, so we restart building... but we don't know if it was
`ninja just-one-target` and so we trigger a rebuild of `all` (potentially
even more annoying than it sounds since `just-one-target` might be excluded
from `all`).

Ideally, we could reuse built-in CONFIGURE_DEPENDS for globbing:
append to `VerifyGlobs.cmake`. However, that script is not written
until all user cmake finishes running:
https://github.com/Kitware/CMake/blob/master/Source/cmake.cxx#L2602

Furthermore: it's overwritten each time cmake runs so even if we
modify it from a wrapper (preserving mtime so that we don't trigger
immediate reconf), we would need to use the wrapper *each time* or
reconfiguration (including reconfigure during build!) will wipe
it out. At that point, we might as well skip modifying anything and
just have the wrapper directly reconfigure.

If we could start a background process which waits for VerifyGlobs.cmake
to be written *then* amends it, that might work. But execute_process
is able to detect fork() child processes somehow even if setsid() is called,
and blocks until they complete. Linking to libuv and using
uv_spawn(UV_PROCESS_DETACHED) instead of fork() does actually put the
spawned process into the background, but acquiring the same libuv linked to
cmake does not sound easy. In particular, cmake could even be statically linked
to libuv... in which case the main uv_loop_t in my backgrounding process would
probably not refer to the same one used by cmake.
Maybe if I `strace` the libuv version, I'll see another syscall which will
help the dependency-free version escape.

AHA in addition to setsid, I need to redirect the output of the command to files
or I guess cmake waits for them to close.

```cmake
cmake_minimum_required(VERSION 3.28)
project(BgWoes)

execute_process(
  COMMAND ${CMAKE_SOURCE_DIR}/uv-do-bg

  # Both of these hang (without both setsid and output redirection):
  #COMMAND ${CMAKE_SOURCE_DIR}/do-bg
  #COMMAND sh ${CMAKE_SOURCE_DIR}/do-bg.sh
  
  OUTPUT_FILE ${MAUD_DIR}/junk
  ERROR_FILE ${MAUD_DIR}/junk

  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMAND_ERROR_IS_FATAL ANY
)
message(STATUS "do-bg")
```

```sh
setsid
sleep 100 &
# OR
# setsid --fork sleep 100
```

```c
#include <uv.h>
int main() {
    uv_loop_t *loop = uv_default_loop();

    char* args[] = {"sleep", "100", NULL};
    uv_process_options_t options{
        .exit_cb = NULL,
        .file = "sleep",
        .args = args,
        /* failure to set this flag makes execute_process block */
        .flags = UV_PROCESS_DETACHED,
    };

    uv_process_t child_req = {0};
    int r;
    if ((r = uv_spawn(loop, &child_req, &options))) {
        fprintf(stderr, "%s\n", uv_strerror(r));
        return 1;
    }

    fprintf(stderr, "Launched sleep with PID %d\n", child_req.pid);
    uv_unref((uv_handle_t*) &child_req);
    return uv_run(loop, UV_RUN_DEFAULT);
}
```

```c++
#include <iostream>
#include <unistd.h>
int main() {
  auto id = fork();
  if (id == -1) {
    std::cerr << "fork error" << std::endl;
    return errno;
  }
  if (id != 0) return 0;
  setsid();
  return std::system("sleep 100");
}
```

Now what do you do for windows? https://learn.microsoft.com/en-us/windows-server/administration/windows-commands/start
`start /b` will run it in the background, I wonder if that'll be enough
