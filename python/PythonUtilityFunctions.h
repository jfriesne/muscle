#ifndef PythonUtilityFunctions_h
#define PythonUtilityFunctions_h

// Python.h is part of the Python2.2.1 distribution, in the Include folder.
// If you want to include PythonUtilityFunctions.h in your code, you'll need
// to make sure that python/Include is in your includes-path.
#include "support/MuscleSupport.h"
#include "Python.h"  

namespace muscle {

/** @defgroup pythonutilityfunctions The PythonUtilityFunctions function API
 *  These functions are all defined in PythonUtilityFunctions(.cpp,.h), and are stand-alone
 *  functions that are useful when embedding a Python interpreter into MUSCLE code.
 *  @{
 */

class Message;
class String;

/** Given the arguments passed by a Python script back to a C callback method, populates the given Message 
 *  with corresponding argument data.  
 *  @note This function is only useful if you are trying to interface Python scripts and MUSCLE C++ code in the same executable.
 *  @param args The args tuple parameter for the positional arguments in the call.  May be NULL.
 *  @param keywords The keywords dictionary parameter for the keyword arguments in the call.  May be NULL.
 *  @param msg The Message object where the argument data will be written to on success.
 *  @return B_NO_ERROR on success, or an error code on failure.
 */
status_t ParsePythonArgs(PyObject * args, PyObject * keywords, Message & msg);

/** Given a Message with the specified field, attempts to return a newly referenced PyObject that
 *  represents the value at the (index)'th position in the field.
 *  @note This function is only useful if you are interfacing Python and MUSCLE C++ code together in the same executable.
 *  @param msg The Message to look into
 *  @param fieldName The field name to look in
 *  @param index The index of the item in the field to use (often zero)
 *  @return A PyObject on success, or NULL (and sets the exception string) on failure.
 */
PyObject * ConvertMessageItemToPyObject(const Message & msg, const String & fieldName, uint32 index);

/** Adds the given PyObject to the given Message, under the given key.
 *  Note that this function will not properly handle all possible Python types, but only
 *  the more straightforward ones, including:  Ints (stored as int32s), LongLongs (stored as int64s)
 *  Floats (stored as floats), Strings (stored as strings), Complex (stored as Points), Unicode
 *  (stored as strings in UTF8 format), dicts (stored as Messages... note that the only the keys/value
 *  pairs that use strings as keys will be stored), and lists (note that if the list items are not
 *  all of the same type, then the list may be stored in a different order)
 *  @param optKey If the length of this string is greater than zero, then this string will be used
 *                as the field name under which data is added to the Message.  If (optKey) is "",
 *                then an appropriate default name will be chosen (see GetDefaultPythonArgFieldName()).
 *  @param pyValue The value to add into the Mesasge.  Should not be NULL.
 *  @param addToMsg The Message to add the data to.
 *  @return B_NO_ERROR on success, or an error code if the function was unable to add the data to the Message.
 */
status_t AddPyObjectToMessage(const String & optKey, PyObject * pyValue, Message & addToMsg);

/** Given a standard data type code (e.g. B_STRING_TYPE) returns the default field name that will
 *  be used in a Message for an arg of that type, if a fieldname wasn't explicitly specified.
 *  @param type a B_*_TYPE value indicating the type to inquire about
 */
const char * GetDefaultPythonArgFieldName(uint32 type);

/** @} */ // end of pythonutilityfunctions doxygen group

} // end namespace muscle

#endif
