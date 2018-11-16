// Test group for some basic math operations
#include "tested.h"

template<> void tested::Case<CASE_COUNTER>(tested::IRuntime* runner)
{
   runner->StartCase("Addition");
   tested::FailIf(2 + 2 != 4, "Addition does not work");
}

template<> void tested::Case<CASE_COUNTER>(tested::IRuntime* runner)
{
   runner->StartCase("Multiplication");
   tested::FailIf(2 * 2 != 4, "Multiplication does not work");
}

// Linker is not going to include this file unless we reference any symbol from it
void LinkMathTests()
{
   static tested::Group x("math", tested::CaseCollector<CASE_COUNTER>::collect, __FILE__);
}
