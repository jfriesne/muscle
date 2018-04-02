## MUSCLE Naming Conventions

```cpp
    #ifndef THE_HEADER_FILE_NAME_IN_ALL_CAPS_H
    #define THE_HEADER_FILE_NAME_IN_ALL_CAPS_H

    #include "support/MuscleSupport.h"

    namespace muscle {

    class ClassNamesAreInCamelCase
    {
    public:
       status_t MethodNamesAreInCamelCase(int parameterNamesAreLowerCamelCase)
       {
          int localVariablesAreLowerCamelCase = 0;

          // Failed?  Prefer returning an error-code (typically B_ERROR) over throwing an exception
          if (SomethingWentWrong()) return B_ERROR;  

          [...]
          return B_NO_ERROR;   // indicates success!
       }

       enum {
          COMPILE_TIME_CONSTANTS_ARE_UPPER_CASE_LIKE_THIS
       };           

    private:
       int _memberVariablesStartWithAnUnderbar;
    };

    extern int _staticAndGlobalVariablesAlsoStartWithAnUnderbar;

    };  // end namespace muscle

    #endif
```
