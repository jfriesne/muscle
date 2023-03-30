#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "message/Message.h"
#include "regex/QueryFilter.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This example demonstrates basic usage of the muscle::QueryFilter class to test whether a Message object matches various test-conditions.\n");
   printf("\n");
}

static void TestTheMessage(const Message & msg, const char * filterDescription, const QueryFilter & qf)
{
   DummyConstMessageRef msgRef(msg);
   LogTime(MUSCLE_LOG_ERROR, "QueryFilter \"%s\" says the Message %s\n", filterDescription, qf.Matches(msgRef, NULL)?"MATCHES":"doesn't match");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   Message testMsg(1234);
   (void) testMsg.AddString("Friendship is", "magic");
   (void) testMsg.AddFloat("pi", 3.14159f);
   (void) testMsg.AddInt32("answer", 42);

   LogTime(MUSCLE_LOG_INFO, "Today's test Message is:\n");
   testMsg.PrintToStream();
   printf("\n");

   // Let's create a QueryFilter that only returns true if the Message
   // has a particular what-code
   WhatCodeQueryFilter whatCodeIs1234(1234);
   WhatCodeQueryFilter whatCodeIs4321(4321);
   TestTheMessage(testMsg, "whatCodeIs1234", whatCodeIs1234);
   TestTheMessage(testMsg, "whatCodeIs4321", whatCodeIs4321);

   // Now a QueryFilter that only matches if the Message contains
   // a field with a particular field name
   ValueExistsQueryFilter piExists("pi");                       // does a field name "pi" exist in the Message?
   ValueExistsQueryFilter fnordExists("fnord");                 // does a field name "fnord" exist in the Message?
   ValueExistsQueryFilter piExistsFloat("pi", B_FLOAT_TYPE);    // does a float-field named "pi" exist?
   ValueExistsQueryFilter piExistsString("pi", B_STRING_TYPE);  // does a String-field named "pi" exist?
   TestTheMessage(testMsg, "piExists", piExists);
   TestTheMessage(testMsg, "fnordExists", fnordExists);
   TestTheMessage(testMsg, "piExistsFloat", piExistsFloat);
   TestTheMessage(testMsg, "piExistsString", piExistsString);

   // Now test a QueryFilter that only matches if the Message contains
   // a particular value in a field
   Int32QueryFilter answerIs42("answer", Int32QueryFilter::OP_EQUAL_TO, 42);
   Int32QueryFilter answerIs37("answer", Int32QueryFilter::OP_EQUAL_TO, 37);
   Int32QueryFilter answerIsNegative("answer", Int32QueryFilter::OP_LESS_THAN, 0);
   Int32QueryFilter answerIsPositive("answer", Int32QueryFilter::OP_GREATER_THAN, 0);
   TestTheMessage(testMsg, "answerIs42", answerIs42);             // does field "answer" contain the value 42?
   TestTheMessage(testMsg, "answerIs37", answerIs37);             // does field "answer" contain the value 37?
   TestTheMessage(testMsg, "answerIsNegative", answerIsNegative); // does field "answer" contain a value less than 0?
   TestTheMessage(testMsg, "answerIsPositive", answerIsPositive); // does field "answer" contain a value greater than 0?

   // Lastly we'll compose a few boolean expressions

   AndQueryFilter answerIs42AndThereIsPi((DummyConstQueryFilterRef(answerIs42)), DummyConstQueryFilterRef(piExists));  // extra parens to avoid most-vexing-parse problem
   TestTheMessage(testMsg, "answerIs42AndThereIsPi", answerIs42AndThereIsPi); // does field "answer" contain 42 AND the field "pi" exists?

   OrQueryFilter answerIs37OrThereIsPi((DummyConstQueryFilterRef(answerIs37)), DummyConstQueryFilterRef(piExists));  // extra parens to avoid most-vexing-parse problem
   TestTheMessage(testMsg, "answerIs37OrThereIsPi", answerIs37OrThereIsPi); // does field "answer" contain 37 OR the field "pi" exists?

   OrQueryFilter answerIs37OrThereIsFnord;
   (void) answerIs37OrThereIsFnord.GetChildren().AddTail(DummyConstQueryFilterRef(answerIs37));
   (void) answerIs37OrThereIsFnord.GetChildren().AddTail(DummyConstQueryFilterRef(fnordExists));
   TestTheMessage(testMsg, "answerIs37OrThereIsFnord", answerIs37OrThereIsFnord); // does field "answer" contain 37 OR the field "fnord" exists?

   return 0;
}
