/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleISubexpressionFactory_h
#define MuscleISubexpressionFactory_h

#include "regex/QueryFilter.h"  // for ConstQueryFilterRef
#include "regex/LexerToken.h"

namespace muscle {

/** Interface for any object that CreateQueryFilterFromExpression() can use to
  * create a QueryFilter to represent the logic corresponding to a just-parsed filter-expression.
  * @note see the section "Building a QueryFilter from an expression-String" at the end of the
  *       <a href="https://public.msli.com/lcs/muscle/muscle/html/Beginners%20Guide.html">Beginner's Guide</a> for details.
  */
class ISubexpressionFactory : public RefCountable
{
public:
   /** Default constructor */
   ISubexpressionFactory() {/* empty */}

   /** Destructor */
   virtual ~ISubexpressionFactory() {/* empty */}

   /** Called when CreateQueryFilterFromExpression() wants to create a QueryFilter representing a sub-expression.
     * @param fieldNameTok the first token in the expression (usually LTOKEN_USERSTRING with a Message-fieldname, although it might alternatively be LTOKEN_WHAT or LTOKEN_EXISTS)
     * @param valueIndexInField the index of the value within the field name that the user specified (e.g. if the user specified "foo:3", this argument would be 3)
     *                          If the user didn't specify any index, this value will be 0.
     * @param infixOpTok the token representing the infix-operation of the sub-expression (e.g. LTOKEN_LT for less-than or LTOKEN_EQ for equals)
     * @param valTok the token representing the target value (e.g. LTOKEN_USERSTRING with string "3", in the expression (blah<3))
     * @param valueTypeHint a B_*_TYPE value indicating what the parser thinks the type of the value ought to be (based on any explicit casts or hints in the
     *                          string's format).  The ISubexpressionFactory may ignore this hint if it knows better.
     * @param optDefaultValue if the user specified a default-target value to match against if no value exists in the Message object, that value is passed in
     *                        as a String argument here.  (e.g. for the expression "foo|bar == baz", this argument would be "bar")
     * @param caseSensitive if true, then string-comparison operations should be case-sensitive.  If false, string-comparison operations should be case-insensitive.
     *                      CreateQueryFilterFromExpression() will always pass true by default, but subclasses of ISubexpressionFactory can pass true if they prefer case-insensitivity.
     * @returns a ConstQueryFilterRef on success, or a NULL ref on failure (e.g. parse error)
     */
   virtual ConstQueryFilterRef CreateSubexpression(const LexerToken & fieldNameTok, uint32 valueIndexInField, const LexerToken & infixOpTok, const LexerToken & valTok, uint32 valueTypeHint, const LexerToken & optDefaultValue, bool caseSensitive) const = 0;
};
DECLARE_REFTYPES(ISubexpressionFactory);

/** This ISubexpressionFactory subclass implements the default logic for the syntax
  * described at the bottom of the muscle/html/BeginnersGuide.html document.
  * An object of this type will be used by default by CreateQueryFilterFromExpression()
  * if no other ISubexpressionFactory is passed to it as an argument.
  */
class DefaultSubexpressionFactory : public ISubexpressionFactory
{
public:
   /** Default constructor */
   DefaultSubexpressionFactory() {/* empty */}

   /** Destructor */
   virtual ~DefaultSubexpressionFactory() {/* empty */}

   virtual ConstQueryFilterRef CreateSubexpression(const LexerToken & fieldNameTok, uint32 valueIndexInField, const LexerToken & infixOpTok, const LexerToken & valTok, uint32 valueTypeHint, const LexerToken & optDefaultValue, bool caseSensitive) const;
};
DECLARE_REFTYPES(DefaultSubexpressionFactory);

} // end namespace muscle

#endif
