/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef LexerToken_h
#define LexerToken_h

#include "util/String.h"

namespace muscle {

/** Enumeration of the various tokens parsed by CreateQueryFilterFromExpression()'s parser */
enum {
   LTOKEN_LPAREN = 0,      /**< ( */
   LTOKEN_RPAREN,          /**< ) */
   LTOKEN_NOT,             /**< ! */
   LTOKEN_LT,              /**< < */
   LTOKEN_GT,              /**< >  */
   LTOKEN_EQ,              /**< == */
   LTOKEN_LEQ,             /**< <= */
   LTOKEN_GEQ,             /**< >= */
   LTOKEN_NEQ,             /**< != */
   LTOKEN_AND,             /**< && */
   LTOKEN_OR ,             /**< || */
   LTOKEN_XOR,             /**< ^ */
   LTOKEN_STARTSWITH,      /**< startswith */
   LTOKEN_ENDSWITH,        /**< endswith */
   LTOKEN_CONTAINS,        /**< contains */
   LTOKEN_ISSTARTOF,       /**< isstartof */
   LTOKEN_ISENDOF,         /**< isendof */
   LTOKEN_ISSUBSTRINGOF,   /**< issubstringof */
   LTOKEN_MATCHES,         /**< matches */
   LTOKEN_MATCHESREGEX,    /**< matchesregex */
   LTOKEN_INT64,           /**< (int64) */
   LTOKEN_INT32,           /**< (int32) */
   LTOKEN_INT16,           /**< (int16) */
   LTOKEN_INT8,            /**< (int8) */
   LTOKEN_BOOL,            /**< (bool) */
   LTOKEN_FLOAT,           /**< (float) */
   LTOKEN_DOUBLE,          /**< (double) */
   LTOKEN_STRING,          /**< (string) */
   LTOKEN_POINT,           /**< (point) */
   LTOKEN_RECT,            /**< (rect) */
   LTOKEN_WHAT,            /**< what */
   LTOKEN_EXISTS,          /**< exists */
   LTOKEN_USERSTRING,      /**< some other token supplied by the user */
   NUM_LTOKENS             /**< guard value */
};

/** This class represents a single token parsed by the CreateQueryFilterFromExpression() parser.
  * It will have a LTOKEN_* value, and (if the token-value is LTOKEN_USERSTRING) also a user-supplied
  * String indicating what the associated String is.
  */
class LexerToken
{
public:
   /** Default constructor */
   LexerToken() : _tok(NUM_LTOKENS), _wasQuoted(false) {/* empty */}

   /** Explicit constructor
     * @param tok an LTOKEN_* value
     */
   LexerToken(uint32 tok) : _tok(tok), _wasQuoted(false) {/* empty */}

   /** Constructor for an LTOKEN_USERSTRING
     * @param valStr a user-supplied String (not including any surrounding quotes)
     * @param wasQuoted true iff the user had placed quotes around the string.  Default to false.
     */
   LexerToken(const String & valStr, bool wasQuoted=false) : _tok(LTOKEN_USERSTRING), _valStr(valStr), _wasQuoted(wasQuoted) {/* empty */}

   /** Returns the LTOKEN_* value associated with this LexerToken object. */
   uint32 GetToken() const {return _tok;}

   /** Returns the user-supplied String (if any) associated with this LexerToken. */
   const String & GetValueString() const {return _valStr;}

   /** Returns a human-readable description of this object's state, for debugging purposes. */
   String ToString() const;

   /** Convenience method:  Given a token like "myfield:4", returns the field name "myfield" and the retIdx=4
     * @param retFieldName on successful return, the field-name is placed here
     * @param retValueIndex on successful return, the value-index within the field is placed here (or 0 if no value-index was specified)
     * @param optRetDefaultValue if non-NULL, the user-specified default-value is placed here on successful return.
     * @returns B_NO_ERROR on success, or some other value on failure.
     */
   status_t ParseFieldName(String & retFieldName, uint32 & retValueIndex, LexerToken * optRetDefaultValue) const {return ParseFieldNameAux(_valStr, retFieldName, retValueIndex, optRetDefaultValue);}

   /** returns the B_*_TYPE code representing our value's type, or B_ANY_TYPE on failure/unknown
     * @param explicitCastType the B_*_TYPE value of the explicit-cast present in our subexpression, if known, or B_ANY_TYPE otherwise
     *                         This value is used to guide our determination of what the string's type is.
     */
   uint32 GetValueStringType(uint32 explicitCastType) const;

   /** Convenience method:  Returns the B_*_TYPE associated with this token if this token is an explicit-cast (e.g. "(int32)", or B_ANY_TYPE otherwise */
   uint32 GetExplicitCastTypeCode() const;

   /** Returns the StringQueryFilter::OP_* value associated with this infix operator, or StringQueryFilter::NUM_STRING_OPERATORS on failure
     * @param isCaseSensitive true for case-sensitive operations, or false for case-insensitive operations
     */
   uint8 GetStringQueryFilterOp(bool isCaseSensitive) const;

   /** Returns the NumericQueryFilter::OP_* value associated with this infix operator, or NumericQueryFilter::NUM_STRING_OPERATORS on failure */
   uint8 GetNumericQueryFilterOp() const;

private:
   uint32 _tok;
   String _valStr;
   bool _wasQuoted;

   status_t ParseFieldNameAux(const String & valStr, String & retFieldName, uint32 & retIdx, LexerToken * optRetDefaultValue) const;
};

} // end namespace muscle

#endif
