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

   try
   {
      const tested::Subset::RunInfo runInfo = allTests.Run();

      printf("\n=======================================================================\n");

      printf("Test run completed:\n");
      printf("   Passed : %d\n", runInfo.Passed);
      printf("   Skipped: %d\n", runInfo.Skipped);
      printf("   Failed : %d\n", runInfo.Failed);

      return (runInfo.Failed != 0) ? RetCode_TestsFailed : RetCode_Ok;

   }
   catch (tested::CollectFailed& collectFailed)
   {
      printf("\n=======================================================================\n");

      printf("Error: %s\n", collectFailed.Message);
      printf("   In    '%s'\n", collectFailed.FileName);
      printf("   Group '%s'\n", collectFailed.GroupName);
      printf("   Case  #%d\n", collectFailed.Ordinal);
      printf("\n");

      return RetCode_FailedToStart;
   }
}