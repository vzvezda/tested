// Test group for std::vector (
#include "tested.h"
#include <vector>

template<> void tested::Case<CASE_COUNTER>(tested::IRuntime* runner)
{
	runner->StartCase("EmptyByDefault");

	std::vector<int> vec;
	tested::Is(vec.empty(), "Vector must be empty by default");
}

template<> void tested::Case<CASE_COUNTER>(tested::IRuntime* runner)
{
	runner->StartCase("AddElement");

	std::vector<int> vec;
	vec.push_back(1);
	tested::Is(vec.size() == 1);
	tested::FailIf(vec.empty());
	tested::Is(vec[0] == 1);
}

void LinkVectorTests()
{
	static tested::Group x("vector", tested::CaseCollector<CASE_COUNTER>::collect, __FILE__);
}
