/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "support/Rect.h"
#include "util/String.h"

using namespace muscle;

#define TEST(x) if (!(x)) printf("Test failed, line %i\n",__LINE__)

typedef Tuple<3,int> MyTuple;

static void PrintMyTuple(const MyTuple & a);
static void PrintMyTuple(const MyTuple & a)
{
   printf("{%i,%i,%i}", a[0], a[1], a[2]);
}

static void PrintEquation1(const char * op, const MyTuple & a, const MyTuple & b, const MyTuple & c);
static void PrintEquation1(const char * op, const MyTuple & a, const MyTuple & b, const MyTuple & c)
{
   PrintMyTuple(a);
   printf(" %s ", op);
   PrintMyTuple(b);
   printf(" = ");
   PrintMyTuple(c);
   printf("\n");
}

static uint32 counter = 0;

// test subclassing
class MyTupleSubclass : public Tuple<5, float>
{
public:
   MyTupleSubclass() {/* empty */}

   explicit MyTupleSubclass(float first) 
   {
      for (uint32 i=0; i<GetNumItemsInTuple(); i++) (*this)[i] = first + 50.0f - (float)i;
      _count = counter++;
   }
 
   void PrintToStream() const
   {
      printf("{");
      for (uint32 i=0; i<GetNumItemsInTuple(); i++) printf("%.1f%s", (*this)[i], (i==GetNumItemsInTuple()-1)?"":",");
      printf("}(c=" UINT32_FORMAT_SPEC ")", _count);
   }

private:
   uint32 _count;
};
DECLARE_ALL_TUPLE_OPERATORS(MyTupleSubclass, float);


static void PrintEquation2(const char * op, const MyTupleSubclass & a, const MyTupleSubclass & b, const MyTupleSubclass & c);
static void PrintEquation2(const char * op, const MyTupleSubclass & a, const MyTupleSubclass & b, const MyTupleSubclass & c)
{
   a.PrintToStream();
   printf(" %s ", op);
   b.PrintToStream();
   printf(" = ");
   c.PrintToStream();
   printf("\n");
}

// test subclassing
class StringTupleSubclass : public Tuple<3, String>
{
public:
   StringTupleSubclass() {/* empty */}

   StringTupleSubclass(const String & s1, const String & s2, const String & s3) 
   {
      (*this)[0] = s1;
      (*this)[1] = s2;
      (*this)[2] = s3;
   }
 
   void PrintToStream() const
   {
      printf("{");
      for (uint32 i=0; i<GetNumItemsInTuple(); i++) printf("[%s]%s", (*this)[i](), (i==GetNumItemsInTuple()-1)?"":",");
      printf("}");
   }

private:
};
DECLARE_ADDITION_TUPLE_OPERATORS(StringTupleSubclass, String);
DECLARE_SUBTRACTION_TUPLE_OPERATORS(StringTupleSubclass, String);

static void PrintEquation3(const char * op, const StringTupleSubclass & a, const StringTupleSubclass & b, const StringTupleSubclass & c);
static void PrintEquation3(const char * op, const StringTupleSubclass & a, const StringTupleSubclass & b, const StringTupleSubclass & c)
{
   a.PrintToStream();
   printf(" %s ", op);
   b.PrintToStream();
   printf(" = ");
   c.PrintToStream();
   printf("\n");
}

static void PrintEquation4(const char * op, const Point & a, const Point & b, const Point & c);
static void PrintEquation4(const char * op, const Point & a, const Point & b, const Point & c)
{
   a.PrintToStream();
   printf(" %s ", op);
   b.PrintToStream();
   printf(" = ");
   c.PrintToStream();
   printf("\n");
}

static void PrintEquation5(const char * op, const Rect & a, const Rect & b, const Rect & c);
static void PrintEquation5(const char * op, const Rect & a, const Rect & b, const Rect & c)
{
   a.PrintToStream();
   printf(" %s ", op);
   b.PrintToStream();
   printf(" = ");
   c.PrintToStream();
   printf("\n");
}

class FiveTuple : public Tuple<5,int>
{
public:
   FiveTuple() {/* empty */}
};
DECLARE_ALL_TUPLE_OPERATORS(FiveTuple, int);

static void PrintFiveTuple(const FiveTuple & ft)
{
   for (uint32 i=0; i<ft.GetNumItemsInTuple(); i++) printf("%c%i", (i==0)?'[':' ', ft[i]);
   printf("]");
}

// This program exercises the Tuple class
int main()
{
   printf("Tuple shift test\n");
   FiveTuple shiftTuple;
   for (uint32 i=0; i<shiftTuple.GetNumItemsInTuple(); i++) shiftTuple[i] = i+10;
   for (int32 leftShift=-10; leftShift<=10; leftShift++)
   {
      PrintFiveTuple(shiftTuple);
      printf(" shifted left " INT32_FORMAT_SPEC " slots, becomes ", leftShift);
      PrintFiveTuple(shiftTuple<<leftShift);
      printf("\n");
   }
   for (int32 rightShift=-10; rightShift<=10; rightShift++)
   {
      PrintFiveTuple(shiftTuple);
      printf(" shifted right " INT32_FORMAT_SPEC " slots, becomes ", rightShift);
      PrintFiveTuple(shiftTuple>>rightShift);
      printf("\n");
   }
   
   printf("\nTest 1, with tuple using 3 ints\n");
   {
      MyTuple a;
      a[0] = 5; a[1] = 10; a[2] = 15;
      MyTuple b;
      b[0] = 1; b[1] = 2; b[2] = -3;
    
      b.Replace(-3, -4);

      printf("a=");   PrintMyTuple(a);   printf("\n");
      printf("a+3="); PrintMyTuple(a+3); printf("\n");
      printf("a-3="); PrintMyTuple(a-3); printf("\n");
      printf("a*3="); PrintMyTuple(a*3); printf("\n");
      printf("a/3="); PrintMyTuple(a/3); printf("\n");

      PrintEquation1("+", a, b, a+b);
      PrintEquation1("-", a, b, a-b);
      PrintEquation1("*", a, b, a*b);
      PrintEquation1("/", a, b, a/b);
      printf("a.b=%i\n", a.DotProduct(b));
      printf("b.a=%i\n", b.DotProduct(a));
      PrintEquation1("++", a, b, a+b+b);
      PrintEquation1("+-", a, b, a+b-b);
      PrintEquation1("u-", a, a, -a);
      printf("max value in a is %i, max in b is %i\n", a.GetMaximumValue(), b.GetMaximumValue());
      printf("min value in a is %i, min in b is %i\n", a.GetMinimumValue(), b.GetMinimumValue());
   }
   printf("\n\nTest 2, with subclass using 5 floats\n");
   {
      MyTupleSubclass a(5.0f);
      MyTupleSubclass b(1.0f);
    
      printf("a=");   a.PrintToStream();     printf("\n");
      printf("a+3="); (a+3).PrintToStream(); printf("\n");
      printf("a-3="); (a-3).PrintToStream(); printf("\n");
      printf("a*3="); (a*3).PrintToStream(); printf("\n");
      printf("a/3="); (a/3).PrintToStream(); printf("\n");

      PrintEquation2("+", a, b, a+b);
      PrintEquation2("-", a, b, a-b);
      PrintEquation2("*", a, b, a*b);
      PrintEquation2("/", a, b, a/b);
      PrintEquation2("++", a, b, a+b+b);
      PrintEquation2("+-", a, b, a+b-b);
      PrintEquation2("u-", a, a, -a);
      printf("a.b=%f\n", a.DotProduct(b));
      printf("b.a=%f\n", b.DotProduct(a));
      printf("max value in a is %f, max in b is %f\n", a.GetMaximumValue(), b.GetMaximumValue());
      printf("min value in a is %f, min in b is %f\n", a.GetMinimumValue(), b.GetMinimumValue());
   }
   printf("\n\nTest 3, with tuple using 3 strings\n");
   {
      StringTupleSubclass a("red", "green", "blue");
      StringTupleSubclass b("light", "grass", "rinse");
    
      printf("a=");   a.PrintToStream();     printf("\n");
      printf("a+'b'="); (a+"b").PrintToStream(); printf("\n");
      printf("a-'b'="); (a-"b").PrintToStream(); printf("\n");

      PrintEquation3("+", a, b, a+b);
      PrintEquation3("-", a, b, a-b);
      PrintEquation3("++", a, b, a+b+b);
      PrintEquation3("+-", a, b, a+b-b);
      printf("max value in a is %s, max in b is %s\n", a.GetMaximumValue()(), b.GetMaximumValue()());
      printf("min value in a is %s, min in b is %s\n", a.GetMinimumValue()(), b.GetMinimumValue()());
   }
   printf("\n\nTest 4, using Points\n");
   {
      Point a(5.0f, 6.0f);
      Point b(2.0f, 3.0f);
    
      printf("a=");   a.PrintToStream();     printf("\n");
      printf("a+3="); (a+3.0f).PrintToStream(); printf("\n");
      printf("a-3="); (a-3.0f).PrintToStream(); printf("\n");
      printf("a*3="); (a*3.0f).PrintToStream(); printf("\n");
      printf("a/3="); (a/3.0f).PrintToStream(); printf("\n");

      PrintEquation4("+", a, b, a+b);
      PrintEquation4("-", a, b, a-b);
      PrintEquation4("*", a, b, a*b);
      PrintEquation4("/", a, b, a/b);
      PrintEquation4("++", a, b, a+b+b);
      PrintEquation4("+-", a, b, a+b-b);
      PrintEquation4("u-", a, a, -a);
      printf("a.b=%f\n", a.DotProduct(b));
      printf("b.a=%f\n", b.DotProduct(a));
      printf("max value in a is %f, max in b is %f\n", a.GetMaximumValue(), b.GetMaximumValue());
      printf("min value in a is %f, min in b is %f\n", a.GetMinimumValue(), b.GetMinimumValue());
   }
   printf("\n\nTest 5, using Rects\n");
   {
      Rect a(5,6,7,8);
      Rect b(5,4,3,2);
    
      printf("a=");   a.PrintToStream();     printf("\n");
      printf("a+3="); (a+3.0f).PrintToStream(); printf("\n");
      printf("a-3="); (a-3.0f).PrintToStream(); printf("\n");
      printf("a*3="); (a*3.0f).PrintToStream(); printf("\n");
      printf("a/3="); (a/3.0f).PrintToStream(); printf("\n");

      PrintEquation5("+", a, b, a+b);
      PrintEquation5("-", a, b, a-b);
      PrintEquation5("*", a, b, a*b);
      PrintEquation5("/", a, b, a/b);
      PrintEquation5("++", a, b, a+b+b);
      PrintEquation5("+-", a, b, a+b-b);
      PrintEquation5("u-", a, a, -a);
      printf("a.b=%f\n", a.DotProduct(b));
      printf("b.a=%f\n", b.DotProduct(a));
      printf("max value in a is %f, max in b is %f\n", a.GetMaximumValue(), b.GetMaximumValue());
      printf("min value in a is %f, min in b is %f\n", a.GetMinimumValue(), b.GetMinimumValue());
   }
   return 0;
}
