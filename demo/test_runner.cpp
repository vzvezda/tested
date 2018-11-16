//  (c) 2018 Vladimir Zvezda
//
//  An example of the console app that can run the tests registered in test libraries.
#include "tested.h"
#include <stdio.h>

//-------------------------------------------------------------------------------------------------
//
//-------------------------------------------------------------------------------------------------
extern void LinkMathTests();
extern void LinkVectorTests();

static void RegisterTests()
{
	LinkMathTests();
	LinkVectorTests();
}

//-------------------------------------------------------------------------------------------------
//
//-------------------------------------------------------------------------------------------------
int main(int argc, const char* argv[])
{
	printf("RunTest: run all registered tests\n\n");
	RegisterTests();

	tested::Subset allTests = tested::Storage::Instance().GetAll();
	//tested::Subset myGroup = allTests.ByGroupName("math");

	enum RetCode_t
	{
		RetCode_Ok = 0,
		RetCode_TestsFailed,
		RetCode_FailedToStart,
	};

	auto runResult = allTests.Run();

	struct Visitor
	{
		void operator()(const tested::CollectFailed& collectFailed)
		{
			printf("Error: %s\n", collectFailed.Message);
			printf("   In    '%s'\n", collectFailed.FileName);
			printf("   Group '%s'\n", collectFailed.GroupName);
			printf("   Case  #%d\n", collectFailed.Ordinal);
			printf("\n");

			ReturnCode = RetCode_FailedToStart;
		}


		void operator()(const tested::Subset::RunInfo& runInfo)
		{
			printf("Test run completed:\n");
			printf("   Passed : %d\n", runInfo.Passed);
			printf("   Skipped: %d\n", runInfo.Skipped);
			printf("   Failed : %d\n", runInfo.Failed);

			ReturnCode = (runInfo.Failed != 0) ? RetCode_TestsFailed : RetCode_Ok;
		}

		enum RetCode_t
		{
			RetCode_Ok = 0,
			RetCode_TestsFailed,
			RetCode_FailedToStart,
		};

		RetCode_t ReturnCode;
	};

	Visitor visitor;

	printf("\n=======================================================================\n");
	std::visit(visitor, runResult);


	return visitor.ReturnCode;
}