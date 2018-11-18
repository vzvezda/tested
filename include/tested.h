//     
//   \|/ Tested
//   /|\ 2018.07.28 Vladimir Zvezda
//
//  The front-end to the test cases library.
//
//  Motivation:
//     * no macros when defnining the test cases
//     * minimize boilerplate code in test files
//     * async test support (Emscripten, not yet ready)
//     * no dynamic memory (bonus)
//
//  Design Ideas:
//
//    * minimal core API that about test cases registration and discovery. 
//
//      It does not provide advanced features like handling segfaults, mocks, XML/CI reports, 
//      sanitizers, leak detection, performance measurements, etc because not every project
//      needs this. Once you have test collected and registered, you can use a backend library
//      where additional features are supported.
//
//    * No dynamic memory used in library
//
//      It appeared that it is possible to go without dymanic memory and I took the opportunity.
//      It has some downsides that it can increase the size of executable, but on the other side   
//      you can use it on platform without dynamic memory and it would have no interfere with 
//      leak detector you may want to use for your tests.
//
//  TODO:
//     * filter by case name and number
//     * dont allow ':' in group names
//     * remove string_view
//     * test filter by callback
//     * generic iteration
//     stop here
//     * tags
//     * Async tests
//     * to c++03
//     * Support parameters
//     * How it works on Android/Emscripten/etc
//
#pragma once

#include <string_view>
#include <algorithm>
#include <stdio.h>
#include <exception>
#include <cstddef>

// With C++17 it is possible to make some project specific configuration
#ifdef __has_include
#if __has_include("tested_config.h")
#include "tested_config.h""
#endif
#endif

#if __cplusplus <= 199711L
#define tested_final 
#define tested_override 
#define tested_noexcept 
#define tested_nullptr NULL
#else
#define tested_final final
#define tested_override override
#define tested_noexcept noexcept
#define tested_nullptr nullptr
#endif

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
// possible, because it is used in recursive type definition and static storage. "signed char"
// means that you can have about 127 tests in each translation unit.
// Ordinal should be signed type (-1 used as recursion end, code below compares it >= 0)
typedef signed char Ordinal_t;

// Because of this "no dynamic memory" bravado, we need a convinient storage to keep some messages
template <size_t SizeP>
struct StringStorage
{
   enum { kMaxSize = SizeP };
   char StorageBuf[SizeP];

   StringStorage() { StorageBuf[0] = 0; }

   StringStorage(std::string_view msg) { Assign(msg); }

   void Assign(std::string_view msg)
   {
      const size_t maxSize = (std::min<size_t>)(SizeP, msg.length());
      std::copy_n(msg.cbegin(), maxSize, StorageBuf);
      StorageBuf[maxSize] = 0;
   }
   bool Empty() const { return StorageBuf[0] == 0; }
   const size_t MaxSize() const { return SizeP; }

   const char* CData() const { return StorageBuf; }
   char* Data() { return StorageBuf; }
};

// This is the runtime API of 'tested' library avaiable to test case via parameter. There are two 
// private implementation for this interface: one is to collect all test function and another is to 
// actually run the tests.
struct IRuntime
{
   // Any test case must invoke StartCase method in the begining. Use literal or 
   // storage with static lifetime as test name and description because API does not make the 
   // copy of the data.
   virtual void StartCase(const char* caseName, const char* description = nullptr) = 0;
   // TODO: virtual callback StartAsyncCase();
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

// Private exception classes
struct CaseIsReal   {}; // thrown by StartCase() when Case<>() is specialized
struct CaseIsStub   {}; // thrown by generic template of Case<>()
struct CaseSkipped  {}; // thrown by test case or size check
struct CaseFiltered {}; // thrown by StartCase() when case is not scheduled for execution

// Private exception thrown when a test case is failed, e.g tested::Fail("Unexpected state")
struct CaseFailed final
{
   CaseFailed() {}
   CaseFailed(std::string_view msg): Message(msg) {}

   StringStorage<1024> Message;
};

// Base class for any exception thrown by Subset::Run(). Public API exception class.
struct TestrunException: public std::exception
{
   const char* GroupName;
   const char* FileName;
   Ordinal_t   Ordinal;

   TestrunException(Ordinal_t ordinal = Ordinal_t()) 
      : GroupName(nullptr), FileName(nullptr), Ordinal(ordinal) {}

   const char* what() const noexcept final { return GetFormattedMessage(); }

protected:
   virtual const char* GetFormattedMessage() const = 0;
   mutable StringStorage<1024> m_formattedMessage;
};

// This is the public exception thrown when test case decided that process state is corrupted and 
// there is no much sense to continue running the other tests.
struct ProcessCorruptedException final: public TestrunException
{
   std::string CaseMessage;

   ProcessCorruptedException(std::string_view message): CaseMessage(message)
   {}

private:
   const char* GetFormattedMessage() const final
   {
      snprintf(m_formattedMessage.Data(), m_formattedMessage.MaxSize(),
         "ProcessCorrupted. Case message: %s. File: '%s', group : %s, case: #%d",
         CaseMessage.c_str(), 
         FileName ? FileName : "",
         GroupName ? GroupName : "", 
         Ordinal);

      return m_formattedMessage.CData();
   }
};

// This is the public exception thrown when test cases collected failed, for example test case 
// does not invoke StartCase() method on start.
struct CollectFailedException final : public TestrunException
{
   const char* Message;

   CollectFailedException() : Message(nullptr) {}

   CollectFailedException(Ordinal_t ordinal, const char* message)
      : TestrunException(ordinal), Message(message)
   { }

private:
   const char* GetFormattedMessage() const final
   {
      snprintf(m_formattedMessage.Data(), m_formattedMessage.MaxSize(),
         "Failed to collect test cases: %s. File: '%s', group: %s, case: #%d",
         Message ? Message : "",
         FileName ? FileName : "",
         GroupName ? GroupName : "", 
         Ordinal);

      return m_formattedMessage.CData();
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
   // TODO: write actual and expected? It is difficult without dynamic memory
   // TODO: don't use reference for trivial types?
   if (!(actual == expected))
      Fail(msg);
}

// Test case invokes this to indicate that the current process state is compromised and no sense to
// run other tests.
inline void ProcessCorrupted(std::string_view msg = std::string_view())
{
   throw ProcessCorruptedException(msg);
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

// Subset: a reference to the tests
struct Subset
{
   static constexpr size_t kMaxGroupName = 64;

   Subset() : m_isCollectFailed(false), m_caseNumberFilter(-1)  {}

   struct Stats
   {
      int Skipped;
      int Failed;
      int Passed;

      Stats() : Skipped(0), Failed(0), Passed(0)
      {}

      bool IsFailed() const { return Failed != 0; }
      bool IsPassed() const { return Failed == 0; }
   };

   // Runs the subset of tests. Can throw the ProcessCorrupted and CollectFailed exceptions.
   Stats Run(IProgressEvents* progressEvents = nullptr) const
   {
	   if (m_isCollectFailed)
		   throw m_collectFailedError;

      StdoutReporter consoleReporter;
      return RunParamChecked(progressEvents == nullptr ? &consoleReporter : progressEvents);
   }

private:
   struct Runtime final: IRuntime
   {
      IProgressEvents* m_progressEvents;
      Ordinal_t        m_currentTestOrdinal;
      const char*      m_caseNameFilter;

      Runtime(IProgressEvents* progressEvents, const char* caseNameFilter)
         : m_progressEvents(progressEvents), m_caseNameFilter(caseNameFilter)
      {}

      void StartCase(const char* testName, const char* description = nullptr) final
      {
         if (m_caseNameFilter[0] != 0 
            && (testName == nullptr || strcmp(m_caseNameFilter, testName) != 0))
         {
            throw CaseFiltered();
         }

         IProgressEvents::StartedCase startedCase;
         startedCase.Name = testName;
         startedCase.Ordinal = m_currentTestOrdinal;
         m_progressEvents->OnCaseStart(startedCase);
      }
   };

   Stats RunParamChecked(IProgressEvents* progressEvents) const
   {
      Runtime runtime(progressEvents, m_caseNameFilter.CData());
      Stats result;

      GroupListEntry *groupCur = m_groupListHead;
      while (groupCur != nullptr)
      {
         if (!GroupExcludedByFilter(groupCur))
            RunGroup(runtime, result, progressEvents, groupCur);

         groupCur = groupCur->Next;
      }

      return result;
   }

   void RunGroup(Runtime& runtime, Stats& result, 
      IProgressEvents* progressEvents, GroupListEntry *groupCur) const
   {
      progressEvents->OnGroupStart(groupCur->Name, 0);
      CaseListEntry *cur = groupCur->CaseListHead;
      try
      {
         while (cur != nullptr)
         {
            if (!CaseExcludedByNumber(cur->Ordinal))
               RunOneCase(&runtime, progressEvents, cur, result);
            cur = cur->Next;
         }
      }
      catch (ProcessCorruptedException& processCorrupted)
      {
         processCorrupted.FileName = groupCur->FileName;
         processCorrupted.GroupName = groupCur->Name;
         progressEvents->OnCaseDone(CaseResult_Failed, processCorrupted.what());
         throw processCorrupted;
      }
   }

   bool CaseExcludedByNumber(Ordinal_t caseNumber) const
   {
      return m_caseNumberFilter >= 0 && m_caseNumberFilter != caseNumber;
   }

   bool GroupExcludedByFilter(GroupListEntry* group) const
   {
      return !m_groupNameFilter.Empty()
         && strcmp(group->Name, m_groupNameFilter.CData()) != 0;
   }

   void RunOneCase(Runtime* runtime,
      IProgressEvents* progressEvents, 
      CaseListEntry *caseEntry,
      Stats& result) const
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
      catch (CaseFiltered)
      {
         // It is not really skipped, it just filtered out. E.g. if you run specific test it does 
         // not mean that other tests skipped, they not run and not even counted in statistics.
      }
      catch (const CaseFailed& ex)
      {
         progressEvents->OnCaseDone(CaseResult_Failed, ex.Message.CData());
         result.Failed += 1;
      }
      catch (ProcessCorruptedException& ex)
      {
         ex.Ordinal = caseEntry->Ordinal;
         throw ex; // handled by upper level
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
   // The default progress reporter to stdout. It makes the tested.h usable without any backend.
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
         case CaseResult_Passed:  printf(" PASSED\n");  break;
         case CaseResult_Failed:  printf(" FAILED\n");  break;
         case CaseResult_Skipped: printf(" SKIPPED\n"); break;
         }
      }
   private:
      StartedCase m_currentCase;
   };

protected:
   bool m_isCollectFailed;
   CollectFailedException m_collectFailedError;

   GroupListEntry* m_groupListHead;
   GroupListEntry* m_groupListTail;

   friend struct Storage;
   StringStorage<kMaxGroupName> m_groupNameFilter;
   StringStorage<kMaxGroupName> m_caseNameFilter;
   Ordinal_t m_caseNumberFilter;
};

// This is the storage that actually 
struct Storage final: private Subset
{
   const Subset& GetAll() const { return *this; }

   Subset ByGroupName(std::string_view groupName) const
   {
      Subset res = (*this);
      res.m_groupNameFilter.Assign(groupName);
      return res;
   }

   Subset ByGroupAndCaseName(std::string_view groupName, std::string_view caseName) const
   {
      Subset res = (*this);
      res.m_groupNameFilter.Assign(groupName);
      res.m_caseNameFilter.Assign(caseName);
      return res;
   }

   Subset ByGroupNameAndCaseNumber(std::string_view groupName, Ordinal_t caseNumber) const
   {
      Subset res = (*this);
      res.m_groupNameFilter.Assign(groupName);
      res.m_caseNumberFilter = caseNumber;
      return res;
   }

   Subset ByAddress(std::string_view address) const
   {
   }

   void AddGroup(GroupListEntry* newGroupEntry)
   {
      if (m_groupListTail == nullptr)
         m_groupListTail = m_groupListHead = newGroupEntry;
      else
         m_groupListTail->Next = newGroupEntry;

      while (m_groupListTail->Next != nullptr)
         m_groupListTail = m_groupListTail->Next;
   }

   void AddCollectionError(const CollectFailedException& cx)
   {
      m_isCollectFailed = true;
      m_collectFailedError = cx;
   }

   // There is a static instance for the storage
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
      catch (CollectFailedException ex)
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
         throw CollectFailedException(N, "Case body does not start with StartTest()");
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
      catch (const CollectFailedException&)
      {
         throw;
      }
      catch (...)
      {
         // Case function thrown something unexpected during the registration. The 
         // first thing case should do is invoke StartCase().
         // With C++11 and dynamic memory we can have exception captured std::current_exception()
         throw CollectFailedException(N, "Case throws something before StartCase()");
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

