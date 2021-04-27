/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "system/SetupSystem.h"
#include "util/Cloneable.h"
#include "util/String.h"

using namespace muscle;

class TestCloneable : public Cloneable
{
public:
   explicit TestCloneable(const String & title) : _title(title) {/* empty */}
   TestCloneable(const TestCloneable & rhs) : _title(rhs._title) {/* empty */}

   virtual String GetTitle() const {return _title;}

   DECLARE_STANDARD_CLONE_METHOD(TestCloneable);

private:
   String _title;
};

class SubclassOfTestCloneable : public TestCloneable
{
public:
   explicit SubclassOfTestCloneable(const String & title) : TestCloneable(title) {/* empty */}
   explicit SubclassOfTestCloneable(const TestCloneable & rhs) : TestCloneable(rhs) {/* empty */}

   virtual String GetTitle() const {return TestCloneable::GetTitle().Prepend("SubclassOf");}

   DECLARE_STANDARD_CLONE_METHOD(SubclassOfTestCloneable);
};

class BrokenSubclassOfTestCloneable : public TestCloneable
{
public:
   explicit BrokenSubclassOfTestCloneable(const String & title) : TestCloneable(title) {/* empty */}
   explicit BrokenSubclassOfTestCloneable(const TestCloneable & rhs) : TestCloneable(rhs) {/* empty */}

   virtual String GetTitle() const {return TestCloneable::GetTitle().Prepend("BrokenSubclassOf");}

   //Deliberately commented out, since I want to test to make sure this error gets caught at runtime
   //DECLARE_STANDARD_CLONE_METHOD(SubclassOfTestCloneable);
};

// This function is here just to ensure things are done polymorphically
TestCloneable * CloneTester(const TestCloneable * c)
{
   return static_cast<TestCloneable *>(c->Clone());
}

// This program exercises the Cloneable interface.
int main(int, char **)
{
   {
      TestCloneable tc1("Foo");
      TestCloneable * tc2 = CloneTester(&tc1);
      printf("A: TestCloneable1=[%s] TestCloneable2=[%s]\n", tc1.GetTitle()(), tc2?tc2->GetTitle()():NULL);
      delete tc2;
   }

   {
      SubclassOfTestCloneable tc1("Bar");
      TestCloneable * tc2 = CloneTester(&tc1);
      printf("B: SubclassOfTestCloneable1=[%s] SubclassOfTestCloneable2=[%s]\n", tc1.GetTitle()(), tc2?tc2->GetTitle()():NULL);
      delete tc2;
   }

   {
      BrokenSubclassOfTestCloneable tc1("Baz");
      TestCloneable * tc2 = CloneTester(&tc1);  // note:  This line is expected to crash with an assertion failure!
      printf("C: BrokenSubclassOfTestCloneable1=[%s] BrokenSubclassOfTestCloneable2=[%s]\n", tc1.GetTitle()(), tc2?tc2->GetTitle()():NULL);
      delete tc2;
   }

   return 0;
}

