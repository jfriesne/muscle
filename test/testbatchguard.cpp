/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "system/SetupSystem.h"
#include "util/BatchOperator.h"

using namespace muscle;

class SimpleBatchOperator : public BatchOperator<>
{
public:
   SimpleBatchOperator() {printf("SimpleBatchOperator ctor %p\n", this);}
   virtual ~SimpleBatchOperator() {printf("SimpleBatchOperator dtor %p\n", this);}

protected:
   virtual void BatchBegins() {printf("SimpleBatchOperator::BatchBegins %p\n", this);}
   virtual void BatchEnds()   {printf("SimpleBatchOperator::BatchEnds %p\n",   this);}
};

class IntBatchOperator : public BatchOperator<int>
{
public:
   IntBatchOperator() {printf("IntBatchOperator ctor %p\n", this);}
   virtual ~IntBatchOperator() {printf("IntBatchOperator dtor %p\n", this);}

protected:
   virtual void BatchBegins(const int & i) {printf("IntBatchOperator::BatchBegins %p i=%i\n", this, i);}
   virtual void BatchEnds(const int & i)   {printf("IntBatchOperator::BatchEnds %p i=%i\n",   this, i);}
};

class TestArgsA
{
public:
   TestArgsA()
      : _s(NULL)
      , _i(0) 
   {
      // empty
   }
   TestArgsA(const char * s, int i) : _s(s), _i(i) {/* empty */}

   void PrintToStream() const {printf("TestArgsA:  [%s] %i\n", _s, _i);}

private:
   const char * _s; 
   int _i;
};

class TestArgsABatchOperator : public BatchOperator<TestArgsA>
{
public:
   TestArgsABatchOperator() {printf("TestArgsABatchOperator ctor %p\n", this);}
   virtual ~TestArgsABatchOperator() {printf("TestArgsABatchOperator dtor %p\n", this);}

protected:
   virtual void BatchBegins(const TestArgsA & args) {printf("TestArgsABatchOperator::BatchBegins %p args=", this); args.PrintToStream();}
   virtual void BatchEnds(const TestArgsA & args)   {printf("TestArgsABatchOperator::BatchEnds %p args=",   this); args.PrintToStream();}
};

class TestArgsB
{
public:
   TestArgsB()
      : _s(NULL)
      , _i(0) 
   {
      // empty
   }
   TestArgsB(const char * s, int i) : _s(s), _i(i) {/* empty */}

   void PrintToStream() const {printf("TestArgsB:  [%s] %i\n", _s, _i);}

private:
   const char * _s; 
   int _i;
};

class TestArgsBBatchOperator : public BatchOperator<TestArgsB>
{
public:
   TestArgsBBatchOperator() {printf("TestArgsBBatchOperator ctor %p\n", this);}
   virtual ~TestArgsBBatchOperator() {printf("TestArgsBBatchOperator dtor %p\n", this);}

protected:
   virtual void BatchBegins(const TestArgsB & args) {printf("TestArgsBBatchOperator::BatchBegins %p args=", this); args.PrintToStream();}
   virtual void BatchEnds(const TestArgsB & args)   {printf("TestArgsBBatchOperator::BatchEnds %p args=",   this); args.PrintToStream();}
};

class CombinedBatchOperator : public TestArgsABatchOperator, public TestArgsBBatchOperator
{
public:
   CombinedBatchOperator() {printf("CombinedBatchOperator %p\n", this);}
   virtual ~CombinedBatchOperator() {printf("~CombinedBatchOperator %p\n", this);}

   // Why are these necessary?  Because C++ is lame, that's why!
   using TestArgsABatchOperator::GetBatchGuard;
   using TestArgsBBatchOperator::GetBatchGuard;
   using TestArgsABatchOperator::BeginOperationBatch;
   using TestArgsBBatchOperator::BeginOperationBatch;
   using TestArgsABatchOperator::EndOperationBatch;
   using TestArgsBBatchOperator::EndOperationBatch;
};

// This program tests the relative speeds of various object allocation strategies.
int main(int, char **)
{
   CompleteSetupSystem css;  // required!

   printf("\n\nSimpleBatchOperator Test --------------\n");
   {
      SimpleBatchOperator bo;
      printf("First guard...\n");
      DECLARE_BATCHGUARD(bo);
      {
         printf("  Second guard...\n");
         DECLARE_BATCHGUARD(bo);
         {
            printf("    Third guard...\n");
            DECLARE_BATCHGUARD(bo);
            printf("    End Third guard...\n");
         }
         printf("  End Second guard...\n");
      }
      printf("End First guard...\n");
   }

   printf("\n\nIntBatchOperator Test --------------\n");

   {
      IntBatchOperator bo;
      printf("First guard...\n");
      DECLARE_BATCHGUARD(bo);
      {
         printf("  Second guard...\n");
         DECLARE_BATCHGUARD(bo);
         {
            printf("    Third guard...\n");
            DECLARE_BATCHGUARD(bo);
            printf("    End Third guard...\n");
         }
         printf("  End Second guard...\n");
      }
      printf("End First guard...\n");
   }

   printf("\n\nTestArgsABatchOperator Test --------------\n");

   {
      TestArgsABatchOperator bo;
      printf("First guard...\n");
      DECLARE_BATCHGUARD(bo, TestArgsA("Hi", 666));
      {
         printf("  Second guard...\n");
         DECLARE_BATCHGUARD(bo, TestArgsA("Bye", 667));
         {
            printf("    Third guard...\n");
            DECLARE_BATCHGUARD(bo);
            printf("    End Third guard...\n");
         }
         printf("  End Second guard...\n");
      }
      printf("End First guard...\n");
   }

   printf("\n\nCombinedBatchOperator Test --------------\n");

   {
      CombinedBatchOperator bo;
      printf("First guard (TestArgsA)...\n");

      printf("  Manual begin (TestArgsA)...\n");
      bo.BeginOperationBatch(TestArgsA("xxx", 123));

      DECLARE_BATCHGUARD(bo, TestArgsA("Hi", 666));
      {
         printf("  Second guard (TestArgsA)...\n");
         DECLARE_BATCHGUARD(bo, TestArgsA("Bye", 667));
         {
            printf("    Third guard (TestArgsB)...\n");
            DECLARE_BATCHGUARD(bo, TestArgsB("CCC", 999));

            printf("  Manual end (TestArgsA)...\n");
            bo.EndOperationBatch(TestArgsA("xxx", 123));

            printf("    End Third guard (TestArgsB)...\n");
         }
         printf("  End Second guard... (TestArgsA)\n");
      }
      printf("End First guard... (TestArgsA)\n");
   }
   return 0;
}
