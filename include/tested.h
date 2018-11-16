//     
//   \|/ Tested
//    |  2018.07.28 Vladimir Zvezda
//
//  The front-end to the test cases library.
//
//  Motivation:
//     * no macros when writing the test cases
//     * minimize boilerplate code in test files
//     * async test support (Emscripten, not yet ready)
//     * no dynamic memory
//
//  Design Ideas:
//
//    * minimal core API that about test cases registration and discovery. 
//
//      No advanced features like handling segfaults, mocks, XML/CI reports, sanitizers, 
//      leak detection, performance measurements, etc. It could be built on top of this library, 
//      either your project specific or a generic one.
//
//    * Written for C++17
//
//      There is nothing from C++17 here that can not be implemented in C++03, but it is 
//      nice to have all these std::string_view, override, etc. It requires C++ exceptions 
//      to be enabled, which is used as test case exit during the test discovery and runs.
//
//    * No dynamic memory used in library
//
//      It appeared that it is possible to go without dymanic memory and I took the opportunity.
//      It has some downsides that it can increase the size of executable, but on the other side 
//      you can use it on platform without dynamic memory and it would have no interfere with 
//      leak detector you may want to use for your tests.
//
//  DONE:
//     * don't use variants
//
//  TODO:
//     * don't use optional
//     * test filter by test name
//     * test filter by callback
//     * iterators
//     * processcorrupted
//     stop here
//     * Async tests
//     * Support parameters
//     * How it works on Android/Emscripten/etc
//
#pragma once

#include <array>
#include <string_view>
#include <algorithm>
#include <stdio.h>

// Compile time counter that helps to define test cases
#if defined (__COUNTER__)
// __COUNTER__ is supported by gcc/msvc/clang, but it is not in C/C++ standard
#  define CASE_COUNTER __COUNTER__
#else
// When using __LINE__ as counter we can have a big binary and long compile time. If you 
// expirience a problem, here is what you can do:
//    * check if there is a __COUNTER__ like macro on your compiler
//    * assign numbers to your tests manually
//    * search for portable constexpr counter trick 
#  define CASE_COUNTER __LINE__
#endif

// Use CASE_LINE in assertions and the line where assertion fails can be printed
#define TESTED_STRINGIFY(x) #x
#define TESTED_TOSTRING(x) TESTED_STRINGIFY(x)
#define CASE_LINE "('" __FILE__ "':" TESTED_TOSTRING(__LINE__) ") "

namespace tested {

enum CaseResult_t
{
   CaseResult_Passed,
   CaseResult_Failed,
   CaseResult_Skipped
};

// Type to use for test case number local to translation unit. It is safer to have it as small as
// possible, because it is used in recursive type definition and static storage. 
typedef signed char Ordinal_t;

// This is the runtime API of 'tested' library avaiable to test case via parameter. There are two 
// private implementation for this interface: one is to collect all test function and another is to 
// actually run the tests.
struct IRuntime
{
   // Any test case must invoke StartCase method in the begining. Use literal or 
   // storage with static lifetime as test name and description because API does not make the 
   // copy of the data.
   virtual void StartCase(const char* caseName, const char* description = nullptr) = 0;
};

// When app starts the test run, it provides impl of this interface to receive the progress of
// the test run.
struct IProgressEvents
{
   virtual ~IProgressEvents() {}

   virtual void OnGroupStart(const char* groupName, int testsInGroup) = 0;

   struct StartedCase
   {
      const char* Name;
      Ordinal_t   Ordinal;
   };

   virtual void OnCaseStart(StartedCase caseInfo) = 0;
   virtual void OnCaseDone(CaseResult_t code, const char* message = nullptr) = 0;
};

struct CaseIsReal  {}; // thrown by StartCase() when Case<>() is specialized
struct CaseIsStub  {}; // thrown by generic template of Case<>()
struct CaseSkipped {}; // thrown by test case or size check

// Exception thrown when a test case is failed
struct CaseFailed final
{
   // Because of goal of avoiding the dynamic memory we use a buffer for error
   static constexpr size_t kMaxMessage = 1024;
   std::array<char, kMaxMessage + 1> Message;

   CaseFailed()
   {
      Message[0] = 0;
   }

   CaseFailed(std::string_view msg)
   {
      // like strncpy but guarantees zero terminatation (but not utf8-safe)
      const size_t maxSize = (std::min)(kMaxMessage, msg.length());
      std::copy_n(msg.cbegin(), maxSize, Message.begin());
      Message[maxSize] = 0;
   }
};

// Test case function template. App test code must specializes this function in separate
// translation units to make a new test case. 
template <Ordinal_t N> static void Case(IRuntime*) { throw CaseIsStub(); };

// Basic test flow control
inline void Skip() { throw CaseSkipped(); }
inline void Fail(std::string_view msg = "") {  throw CaseFailed(msg); }
inline void FailIf(bool condition, std::string_view msg = "") { if (condition) Fail(msg); }
inline void Is(bool condition, std::string_view msg = "")     { FailIf(!condition, msg); }
inline void Not(bool condition, std::string_view msg = "")    { FailIf(condition, msg); }

template <typename ActualT, typename ExpectedT>
inline void Eq(const ActualT& actual, const ExpectedT& expected, std::string_view msg="")
{
   // TODO: write actual and expected?
   if (!(actual == expected))
      Fail(msg);
}

inline void Panic(std::string_view msg = std::string_view()) 
{
   // TODO: hard mode
   throw CaseFailed(msg);
}

// Pointer to test case function
typedef void (*CaseProc_t)(IRuntime*);

// All test cases within given translation unit linked using this list
struct CaseListEntry final
{
   CaseListEntry* Next;
   CaseProc_t     CaseProc;
   Ordinal_t      Ordinal;
};

// 
struct GroupListEntry
{
   GroupListEntry* Next;
   const char*     Name;
   const char*     FileName;
   CaseListEntry*  CaseListHead;

   GroupListEntry(const char* name, const char* fileName)
      : Next(nullptr), Name(name), FileName(fileName), CaseListHead(nullptr)
   {}
};

struct CollectFailed final
{
   const char* Message;
   const char* GroupName;
   const char* FileName;
   Ordinal_t   Ordinal;

   CollectFailed()
      : Message(nullptr)
      , GroupName(nullptr)
      , FileName(nullptr)
      , Ordinal()
   {}

   CollectFailed(Ordinal_t ordinal, const char* message)
      : Message(message)
      , GroupName(nullptr)
      , FileName(nullptr)
      , Ordinal(ordinal)
   {}
};


// Subset: a reference to the tests
struct Subset
{
   static constexpr size_t kMaxGroupName = 64;

   Subset() : m_collectFailed(false)
   {
      m_GroupNameFilter[0] = 0;
   }

   Subset ByGroupName(std::string_view name) const
   {
      Subset res = (*this);

      auto last = std::copy_n(name.cbegin(), 
         (std::min)(name.length(), kMaxGroupName - 1), 
         res.m_GroupNameFilter.begin());

      *last = 0;

      return res;
   }

   /*
   Subset ByCaseName(std::string_view) const
   {
   }
   */

   struct RunInfo
   {
      int Skipped;
      int Failed;
      int Passed;

      RunInfo() : Skipped(0), Failed(0), Passed(0)
      {}

      bool IsFailed() const { return Failed != 0; }
      bool IsPassed() const { return Failed == 0; }
   };

   RunInfo Run(IProgressEvents* progressEvents = nullptr) const
   {
	   if (m_collectFailed)
		   throw m_collectFailedError;

      StdoutReporter consoleReporter;
      return RunParamChecked(progressEvents == nullptr ? &consoleReporter : progressEvents);
   }

private:
   struct Runtime final: IRuntime
   {
      IProgressEvents* m_progressEvents;
      Ordinal_t m_currentTestOrdinal;

      Runtime(IProgressEvents* progressEvents)
         : m_progressEvents(progressEvents)
      {}

      virtual void StartCase(const char* testName,
         const char* description = nullptr) override final
      {
         IProgressEvents::StartedCase startedCase;

         startedCase.Name = testName;
         startedCase.Ordinal = m_currentTestOrdinal;
         m_progressEvents->OnCaseStart(startedCase);
      }
   };

   RunInfo RunParamChecked(IProgressEvents* progressEvents) const
   {
      Runtime runtime(progressEvents);
      RunInfo result;

      GroupListEntry *groupCur = m_groupListHead;
      while (groupCur != nullptr)
      {
         if (!GroupExcludedByFilter(groupCur))
            RunGroup(runtime, result, progressEvents, groupCur);

         groupCur = groupCur->Next;
      }

      return result;
   }

   void RunGroup(Runtime& runtime, RunInfo& result, 
      IProgressEvents* progressEvents, GroupListEntry *groupCur) const
   {
      progressEvents->OnGroupStart(groupCur->Name, 0);
      CaseListEntry *cur = groupCur->CaseListHead;
      while (cur != nullptr)
      {
         RunOneCase(&runtime, progressEvents, cur, result);
         cur = cur->Next;
      }
   }

   bool GroupExcludedByFilter(GroupListEntry* group) const
   {
      return m_GroupNameFilter[0] != 0
         && strcmp(group->Name, m_GroupNameFilter.data()) == 0;
   }

   void RunOneCase(Runtime* runtime,
      IProgressEvents* progressEvents, 
      CaseListEntry *caseEntry,
      RunInfo& result) const
   {
      try
      {
         runtime->m_currentTestOrdinal = caseEntry->Ordinal;
         caseEntry->CaseProc(runtime);
         progressEvents->OnCaseDone(CaseResult_Passed);
         result.Passed += 1;
      }
      catch (CaseSkipped)
      {
         progressEvents->OnCaseDone(CaseResult_Skipped);
         result.Skipped += 1;
      }
      catch (CaseFailed& ex)
      {
         progressEvents->OnCaseDone(CaseResult_Failed, ex.Message.data());
         result.Failed += 1;
      }
      catch (std::exception& ex)
      {
         progressEvents->OnCaseDone(CaseResult_Failed, ex.what());
      }
      catch (...)
      {
         progressEvents->OnCaseDone(CaseResult_Failed, "Unknown exception");
      }
   }

private:
   // The default progress reporter
   struct StdoutReporter final: IProgressEvents  
   {
      void OnGroupStart(const char* groupName, int testsInGroup) override
      {
         printf("\n%s [group]\n", groupName);
         printf("-----------------------------------------------------------------------\n\n");
      }

      void OnCaseStart(StartedCase caseInfo) override
      {
         m_currentCase = caseInfo;
         printf("%02d:%s...\n", caseInfo.Ordinal, caseInfo.Name);
      }

      void OnCaseDone(CaseResult_t code, const char* message) override
      {
         if (code == CaseResult_Failed && message != nullptr && message[0] != 0)
            printf("Case failed: %s\n", message);
         if (code == CaseResult_Skipped && message != nullptr && message[0] != 0)
            printf("Case skipped: %s\n", message);

         printf("%02d:%s", m_currentCase.Ordinal, m_currentCase.Name);
         switch (code)
         {
         case CaseResult_Passed:      printf(" PASSED\n");  break;
         case CaseResult_Failed:  printf(" FAILED\n");  break;
         case CaseResult_Skipped: printf(" SKIPPED\n"); break;
         }
      }
   private:
      StartedCase m_currentCase;
   };

protected:
   bool          m_collectFailed;
   CollectFailed m_collectFailedError;

   GroupListEntry* m_groupListHead;
   GroupListEntry* m_groupListTail;

   std::array<char, kMaxGroupName> m_GroupNameFilter;
};


// This is the storage that actually 
struct Storage final: private Subset
{
   const Subset& GetAll() const { return *this; }

   void AddGroup(GroupListEntry* newGroupEntry)
   {
      if (m_groupListTail == nullptr)
         m_groupListTail = m_groupListHead = newGroupEntry;
      else
         m_groupListTail->Next = newGroupEntry;

      while (m_groupListTail->Next != nullptr)
         m_groupListTail = m_groupListTail->Next;
   }

   void AddCollectionError(const CollectFailed& cx)
   {
      m_collectFailed = true;
      m_collectFailedError = cx;
   }

   static Storage& Instance()
   {
      static Storage s_storage;
      return s_storage;
   }
};

// API to define the test group
struct Group final: private GroupListEntry
{
public:
   Group(const char* groupName, 
      CaseListEntry* (*collectCasesProc)(CaseListEntry*),
      const char* fileName,
      Storage& storage = Storage::Instance())
      : GroupListEntry(groupName, fileName)
   {
      try
      {
         CaseListHead = collectCasesProc(nullptr);
         storage.AddGroup(this);
      }
      catch (CollectFailed ex)
      {
         ex.GroupName = groupName;
         ex.FileName = fileName;
         storage.AddCollectionError(ex);
      }
   }
};


// Make the anonymouse namespace to have instances be hidden to specific translation unit
namespace {

template <Ordinal_t N>
struct CaseCollector
{
   // Test runtime that collect the test case
   struct CollectorRuntime final: IRuntime
   {
      virtual void StartCase(const char* caseName,
         const char* description = nullptr) override final
      {
         // the trick is exit from test case function into the collector via throw
         throw CaseIsReal();
      }
   };

   // Finds the Case<N> function in current translation unit and adds into the static list. It uses the 
   // reverse order, so the case executed in order of appearance in C++ file.
   static CaseListEntry* collect(CaseListEntry* tail)
   {
      CaseListEntry* current = nullptr;

      CollectorRuntime collector;
      try
      {
         Case<N>(&collector);
         throw CollectFailed(N, "Case does not started with StartTest()\n");
      }
      catch (CaseIsStub)
      {
         // Case<N> is not implemented, do not add it into the list
         current = tail;
      }
      catch (CaseIsReal)
      {
         s_caseListEntry.CaseProc = Case<N>;
         s_caseListEntry.Next     = tail;
         s_caseListEntry.Ordinal  = N;
         current = &s_caseListEntry;
      }
      catch (const CollectFailed&)
      {
         throw;
      }
      catch (...)
      {
         // Case function thrown something unexpected during the registration. The 
         // first thing case should do is invoke StartCase().
         // With C++11 and dynamic memory we can have exception captured std::current_exception()
         throw CollectFailed(N, "Case throws something before StartCase()");
      }

      return CaseCollector<N - 1>::collect(current);
   }

private:
   static CaseListEntry s_caseListEntry;
};

// This static storage will be instantiated in any cpp file
template <Ordinal_t N> CaseListEntry CaseCollector<N>::s_caseListEntry;

// End of template recursion
template <> struct CaseCollector<-1>
{
   static CaseListEntry* collect(CaseListEntry* tail)
   { return tail;  }
};

} // namespace {


} // namespace tested {

