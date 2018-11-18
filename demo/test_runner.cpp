//  (c) 2018 Vladimir Zvezda
//
//  An example of the console app that can run the tests registered in test libraries.
#include "tested.h"
#include <stdio.h>

//-------------------------------------------------------------------------------------------------
// We need to reference a symbol from test libraries or linker will not include test groups
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
   RegisterTests();

   printf("test_runner: running all registered tests\n\n");

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
      const tested::Subset::Stats runInfo = allTests.Run();

      printf("\n=======================================================================\n");

      printf("Test run completed:\n");
      printf("   Passed : %d\n", runInfo.Passed);
      printf("   Skipped: %d\n", runInfo.Skipped);
      printf("   Failed : %d\n", runInfo.Failed);

      return (runInfo.Failed != 0) ? RetCode_TestsFailed : RetCode_Ok;
   }
   catch (const tested::ProcessCorruptedException& processCorrupted)
   {
      printf("\n=======================================================================\n");
      printf("Test case has reported that process state can be corrupted\n");
      printf("   %s\n", processCorrupted.CaseMessage.c_str());
      printf("   In    '%s'\n", processCorrupted.FileName);
      printf("   Group '%s'\n", processCorrupted.GroupName);
      printf("   Case  #%d\n", processCorrupted.Ordinal);
   }
   catch (const tested::CollectFailedException& collectFailed)
   {
      printf("\n=======================================================================\n");

      printf("Failed to collect test cases: %s\n", collectFailed.Message);
      printf("   In    '%s'\n", collectFailed.FileName);
      printf("   Group '%s'\n", collectFailed.GroupName);
      printf("   Case  #%d\n", collectFailed.Ordinal);
      printf("\n");

      return RetCode_FailedToStart;
   }
}