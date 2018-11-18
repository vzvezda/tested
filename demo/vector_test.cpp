// Test group for std::vector (illustrative purposes)
#include "tested.h"
#include <vector>

template<> void tested::Case<CASE_COUNTER>(tested::IRuntime* runner)
{
   runner->StartCase("emptiness");

   //tested::ProcessCorrupted("Sorry");
   //tested::Fail("Vector must be empty by default");

   std::vector<int> vec;
   tested::Is(vec.empty(), "Vector must be empty by default");
}

template<> void tested::Case<CASE_COUNTER>(tested::IRuntime* runner)
{
   runner->StartCase("AddElement");

   std::vector<int> vec;
   vec.push_back(1);
   tested::Is(vec.size() == 1);
   tested::Is(vec[0] == 1);

   tested::FailIf(vec.empty());
}

void LinkVectorTests()
{
   static tested::Group x("std.vector", tested::CaseCollector<CASE_COUNTER>::collect, __FILE__);
}
