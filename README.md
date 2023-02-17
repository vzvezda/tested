# Tested: Front-end library for test cases
This library has started to improve the no-macro [tut-framework](https://github.com/mrzechonek/tut-framework) unit test framework to have even less boilerplate code. So let write the tests for `std::vector` with `tested` lib, vector_test.cpp:

```c++
// Test group for std::vector (illustrative purposes)
#include "tested.h"
#include <vector>

template<> void tested::Case<CASE_COUNTER>(tested::IRuntime* runtime)
{
   runtime->StartCase("EmptyOnConstruction");
   std::vector<int> vec;
   tested::Is(vec.empty(), "Vector must be empty by default");
}

template<> void tested::Case<CASE_COUNTER>(tested::IRuntime* runtime)
{
   runtime->StartCase("AddElement");

   std::vector<int> vec;
   vec.push_back(1);
   tested::Is(vec.size() == 1);
   tested::Is(vec[0] == 1);
   tested::FailIf(vec.empty());
}

void LinkVectorTests()
{
   static tested::Group<CASE_COUNTER> x("std.vector", __FILE__);
}
```

Unlike the libraries Google Test, Catch2, Boost.Test that heavily depend on macros in test case specification, this library attempts to use the template magic for the same purpose. 

Also in the library:

* No macros* to define test cases
* Exception are required to run test cases
* No dynamic memory allocations to create test case catalog and run the tests
* C++17
* Header-only front-end



The following was supposed to be a future work:

* The support for C++98 

* Async tests for environment like Emscripten where blocking may not be possible

* Better selection of tests to run (e.g. run only small tests or integrational, run tests with specific regexp template, etc)

  

If you understand Russian you can read more about the motivation and implementation detail on https://habr.com/ru/post/434906/. 



Current status: I am currently not having personal projects in development for this library and there is not too many interest, so this is in kind of limbo. 

### Template magic explanation

Somewhere inside the `tested.h` the template function is defined:

```c++
template <Ordinal_t N> static void Case(IRuntime* runtime) { throw TheCaseIsAStub(); }
```

The template argument is an integer type (the `signed char` in current implementation). Therefore when someone define a test like:

```c++
template<> void tested::Case<1>(tested::IRuntime* runtime)
{
   runtime->StartCase("TestCaseName");
   ...
}
```

In C++ it introduces the specialization for template function with template parameter=1 in current translation unit.  The `static` attribute to function indicates that this specialization is not going to collide with specialization in other translation unites. 

So then there is a phase when `tested` creates the test cases catalog. Test cases in translation unit has to be collected into the group that is defined like this:

```c++
static tested::Group<3> x("std.vector", __FILE__);
```

The constructor of the `tested::Group` can invoke methods like `Case<1>`, `Case<2>` and the depending on if inside the case is `throw TheCaseIsAStub();` or `runtime->StartCase("TestCaseName")` it either ignores the function or adds address of `Case<>`  case name into the test catalog. Note that in both cases the exceptions are used.

When the test starter app runs it can get access to the test catalog by invoking the `tested::Storage::Instance().GetAll();` and there is API to run the tests and reported progress, see the `demo/test_runner.cpp` for example.

Also worth noting:

* *There still an optional but convenient macro, the `CASE_COUNTER`, which is by default is the `__COUNTER__` macro that is supported by msvc, clang, gcc. This provide good ergonomic that you don't have assign numbers yourself.  

* All tests in translation unit are captured into the group by `tested::Group`:

  ```c++
  static tested::Group<3> x("std.vector", __FILE__);
  ```

  The object `x` is not really used elsewhere (this is a static object), but what is important is the side effect produced by the constructor of the `tested::Group`. Unfortunately the C++ linker can completely omit the inclusion of the translation unit into the final binary if there are no references to any symbol in this translation unit from the rest of program. One solution could be to define a function and invoke it from your `main()`:

  ```c++
  void LinkVectorTests()
  {
     static tested::Group<CASE_COUNTER> x("std.vector", __FILE__);
  }
  ```

  This would make the linker think that this translation unit is not useless and should be included in the binary and the construction will be invoked. Also group construction happens not before the `main()`. 

  These is also a method with linker flags like `--whole-archive`, see https://stackoverflow.com/questions/805555.

  
