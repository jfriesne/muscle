#ifdef __APPLE__
# include <machine/endian.h>  // gotta do this or it won't compile under OS/X Tiger
#endif

#include "python/PythonUtilityFunctions.h"
#include "message/Message.h"

namespace muscle {

static const char * fname(const String & key, const char * defaultFieldName) {return (key.HasChars()) ? key() : defaultFieldName;}

static status_t ParsePythonDictionary(PyObject * keywords, Message & msg);  // forward declaration
static status_t ParsePythonSequence(  PyObject * args,     Message & msg);  // forward declaration

enum {
   MESSAGE_PYTHON_LIST = 0,
   MESSAGE_PYTHON_DICTIONARY,
   NUM_PYTHON_MESSAGE_TYPES
};

const char * GetDefaultPythonArgFieldName(uint32 type)
{
   switch(type)
   {
      case B_BOOL_TYPE:   case B_INT8_TYPE:   case B_INT16_TYPE: case B_INT32_TYPE: return "_argInt32";
      case B_FLOAT_TYPE:  case B_DOUBLE_TYPE:                                       return "_argFloat";
      case B_STRING_TYPE:                                                           return "_argString";
      case B_POINT_TYPE:                                                            return "_argPoint";
      case B_MESSAGE_TYPE:                                                          return "_argMessage";
      default:                                                                      return NULL;
   }
}

status_t AddPyObjectToMessage(const String & optKey, PyObject * pyValue, Message & msg)
{
   status_t ret = B_ERROR;

   if (pyValue == Py_None) return B_NO_ERROR;  // None is the same as not specifying the arg

        if (PyInt_Check    (pyValue)) ret = msg.AddInt32( fname(optKey, "_argInt32"),  PyInt_AS_LONG(pyValue));
   else if (PyLong_Check   (pyValue)) ret = msg.AddInt64( fname(optKey, "_argInt64"),  PyLong_AsLongLong(pyValue));
   else if (PyFloat_Check  (pyValue)) ret = msg.AddFloat( fname(optKey, "_argFloat"),  (float)PyFloat_AsDouble(pyValue));
   else if (PyString_Check (pyValue)) ret = msg.AddString(fname(optKey, "_argString"), PyString_AsString(pyValue));
   else if (PyComplex_Check(pyValue)) ret = msg.AddPoint( fname(optKey, "_argPoint"),  Point((float)PyComplex_RealAsDouble(pyValue), (float)PyComplex_ImagAsDouble(pyValue)));
   else if (PyUnicode_Check(pyValue))
   {
      PyObject * utf8 = PyUnicode_AsUTF8String(pyValue);
      if (utf8)
      {
         ret = msg.AddString(fname(optKey, "_argString"), PyString_AsString(utf8));
         Py_DECREF(utf8);
      }
      else ret = B_ERROR;
   }
   else if (PyDict_Check(pyValue))
   {
      MessageRef subMsg = GetMessageFromPool();
      if ((subMsg())&&(msg.AddMessage(fname(optKey, "_argMessage"), subMsg) == B_NO_ERROR)) ret = ParsePythonDictionary(pyValue, *subMsg());
   }
   else if (PySequence_Check(pyValue))
   {
      MessageRef subMsg = GetMessageFromPool();
      if ((subMsg())&&(msg.AddMessage(fname(optKey, "_argMessage"), subMsg) == B_NO_ERROR)) ret = ParsePythonSequence(pyValue, *subMsg());
   }
   return ret;
}

static status_t ParsePythonSequence(PyObject * args, Message & msg)
{
   msg.what = MESSAGE_PYTHON_LIST;
   int seqLen = PySequence_Fast_GET_SIZE(args);
   for (int i=0; i<seqLen; i++) if (AddPyObjectToMessage("", PySequence_Fast_GET_ITEM(args, i), msg) != B_NO_ERROR) return B_ERROR;
   return B_NO_ERROR;
}

static status_t ParsePythonDictionary(PyObject * keywords, Message & msg)
{
   msg.what = MESSAGE_PYTHON_DICTIONARY;
   status_t ret = B_NO_ERROR;
   PyListObject * keys = (PyListObject *) PyDict_Keys(keywords);
   if (keys)
   {
      PyListObject * values = (PyListObject *) PyDict_Values(keywords);
      if (values)
      {
         int numKeys   = PyList_GET_SIZE(keys);
         int numValues = PyList_GET_SIZE(values);
         if (numKeys == numValues)
         {
            ret = B_NO_ERROR;
            for (int i=0; i<numKeys; i++)
            {
               PyObject * key = PyList_GET_ITEM(keys, i);
               if (PyString_Check(key))
               {
                  PyObject * value = PyList_GET_ITEM(values, i);
                  if (AddPyObjectToMessage(PyString_AS_STRING(key), value, msg) != B_NO_ERROR)
                  {
                     ret = B_ERROR;
                     break;
                  }
               }
            }
         }
         Py_DECREF(values);
      }
      Py_DECREF(keys);
   }
   return ret;
}

status_t ParsePythonArgs(PyObject * args, PyObject * keywords, Message & msg)
{
   status_t ret = B_NO_ERROR;
   if ((ret == B_NO_ERROR)&&(args)    &&(PySequence_Check(args))) ret = ParsePythonSequence(args, msg);
   if ((ret == B_NO_ERROR)&&(keywords)&&(PyDict_Check(keywords))) ret = ParsePythonDictionary(keywords, msg);
   if (ret != B_NO_ERROR) PyErr_SetString(PyExc_RuntimeError, "Error parsing args into Message format");
   return ret;
}

PyObject * ConvertMessageItemToPyObject(const Message & msg, const String & fieldName, uint32 index)
{
   uint32 type;
   if (msg.GetInfo(fieldName, &type) == B_NO_ERROR)
   {
      bool setErr = false;
      switch(type)
      {
         case B_BOOL_TYPE:
         {
            bool temp;
            if (msg.FindBool(fieldName, index, temp) == B_NO_ERROR) return PyInt_FromLong(temp?1:0);
         }
         break;

         case B_DOUBLE_TYPE:
         {
            double temp;
            if (msg.FindDouble(fieldName, index, temp) == B_NO_ERROR) return PyFloat_FromDouble(temp);
         }
         break;

         case B_FLOAT_TYPE:
         {
            float temp;
            if (msg.FindFloat(fieldName, index, temp) == B_NO_ERROR) return PyFloat_FromDouble(temp);
         }
         break;

         case B_INT64_TYPE:
         {
            int64 temp;
            if (msg.FindInt64(fieldName, index, temp) == B_NO_ERROR) return PyLong_FromLongLong(temp);
         }
         break;

         case B_INT32_TYPE:
         {
            int32 temp;
            if (msg.FindInt32(fieldName, index, temp) == B_NO_ERROR) return PyInt_FromLong(temp);
         }
         break;

         case B_INT16_TYPE:
         {
            int16 temp;
            if (msg.FindInt16(fieldName, index, temp) == B_NO_ERROR) return PyInt_FromLong(temp);
         }
         break;

         case B_INT8_TYPE:
         {
            int8 temp;
            if (msg.FindInt8(fieldName, index, temp) == B_NO_ERROR) return PyInt_FromLong(temp);
         }
         break;

         case B_POINT_TYPE:
         {
            Point temp;
            if (msg.FindPoint(fieldName, index, temp) == B_NO_ERROR) return PyComplex_FromDoubles(temp[0], temp[1]);
         }
         break;

         case B_STRING_TYPE:
         {
            const char * temp;
            if (msg.FindString(fieldName, index, temp) == B_NO_ERROR) return PyString_FromString(temp);
         }
         break;

         case B_RAW_TYPE:
         {
            const void * temp;
            uint32 size;
            if (msg.FindData(fieldName, index, B_RAW_TYPE, &temp, &size) == B_NO_ERROR) return PyString_FromStringAndSize((const char *)temp, (int)size);
         }
         break;

         case B_MESSAGE_TYPE:
         {
            MessageRef subMsg;
            if (msg.FindMessage(fieldName, index, subMsg) == B_NO_ERROR)
            {
               PyObject * ret = (subMsg()->what == MESSAGE_PYTHON_LIST) ? PyList_New(0) : PyDict_New();
               if (ret)
               {
                  for (MessageFieldNameIterator iter(*subMsg(), B_ANY_TYPE, HTIT_FLAG_NOREGISTER); iter.HasData(); iter++)
                  {
                     uint32 j = 0;
                     while(true)
                     {
                        PyObject * sub = ConvertMessageItemToPyObject(*subMsg(), iter.GetFieldName(), j++);
                        if (sub)
                        {
                           if (subMsg()->what == MESSAGE_PYTHON_LIST)
                           {
                              if (PyList_Append(ret, sub) != 0) {Py_DECREF(sub);}
                           }
                           else
                           {
                              if (PyDict_SetItemString(ret, (char *) iter.GetFieldName()(), sub) != 0) {Py_DECREF(sub);}
                           }
                        }
                        else break;
                     }
                  }
                  return ret;
               }
            }
         }
         break;

         default:
            PyErr_SetString(PyExc_RuntimeError, String("Message contained unsupported datatype (field=[%1] index=%2 type=%3)").Arg(fieldName).Arg(index).Arg(type)());
            setErr = true;
         break;
      }
      if (setErr == false) PyErr_SetString(PyExc_RuntimeError, String("Message item [%1] item not found (field=[%1], index=%2)").Arg(fieldName).Arg(index)());
   }
   else PyErr_SetString(PyExc_RuntimeError, String("Field name [%1] not found in Message object").Arg(fieldName())());

   return NULL;
}

}; // end namespace muscle

