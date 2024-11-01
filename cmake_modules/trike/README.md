Simple doc comments for C++
===========================

A Sphinx extension which scans C++ sources and headers
for `///` comments. [libclang](https://libclang.readthedocs.io)
is used to associate these with declarations. These can then be
referenced using the `.. trike-put::` directive.

For example, given the following C++ and rst sources in your project:

```c++
/// Frobnicates the :cpp:var:`whatsit` register.
void frobnicate();
```

```rst
.. trike-put:: cpp:function frobnicate
```

... a [`cpp:function`](https://www.sphinx-doc.org/en/master/usage/domains/cpp.html#directive-cpp-function)
directive will be introduced with content drawn from the `///`

The content of `///` is interpreted as ReStructuredText, so
they can be as expressive as the rest of your documentation. Of particular
note for those who have used other apidoc systems: cross references from
`///` comments to labels defined in `*.rst` (or other `///`a) will just work.
