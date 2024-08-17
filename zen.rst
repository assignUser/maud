Zen
---

- Do not require enumeration of source files; use globs.
- Do not require specification of targets; infer these from exported modules.
- Do not require finding/linking to libraries; infer these from imported modules.
- Do not require install manifests; generate and install what's necessary.
- Do not lock authors into learning maud equivalents for what's already
  available in cmake; reverting to configuration is easy.
- Do not promise dependency management; there is no single best answer to this
  (and there are plenty of okay-ish answers, and a few people stuck with poor ones).

.. cpp:var:: int a = 42

.. cpp:function:: int f(int i)

An expression: :cpp:expr:`a * f(a)`

.. c:macro:: TEST_(case_name, parameters...)

  Defines and registers a test case with optional parameters.
 
  :param case_name: The test case's name
  :param parameters: Parameters with which to parameterize the test body

  If no parameters are provided, a single
  :gtest:`simple test case <primer.html#simple-tests>`
  is defined.

  .. code-block:: c++

    TEST_(basic) {
      // assertions etc...
    }
 
  If parameters are provided, each is wrapped into a
  distinct test case using the same test body. In the scope of
  the test body, the parameter is declared as

  .. cpp:var:: Parameter const &parameter
 
  If parameters are read from an initializer list or other
  range then this is analogous to a [value parameterized test].

  .. code-block:: c++

    TEST_(value_parameterized, {2, 3, 47, 8191}) {
      EXPECT_(is_prime(parameter));
    }
  
  Parameters may also differ in type if they are read from a tuple,
  analogous to a [type parameterized test].

  .. code-block:: c++

    TEST_(type_parameterized, 0, std::string("")) {
      EXPECT_(parameter + parameter == parameter);
    }
 
  Each parameter is [printed] and incorporated into the test case’s
  total name along with case_name and the suite’s name to make it
  accessible to [filtering].
 
  advanced.html#value-parameterized-tests
  advanced.html#type-parameterized-tests
  advanced.html#teaching-googletest-how-to-print-your-values
  advanced.html#running-a-subset-of-the-tests
