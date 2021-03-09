## MUSCLE Naming Conventions

```cpp
    #ifndef TheHeaderFileName_h
    #define TheHeaderFileName_h

    #include "support/MuscleSupport.h"

    namespace muscle {

    class ClassNamesAreInCamelCase
    {
    public:
       status_t MethodNamesAreInCamelCase(int parameterNamesAreLowerCamelCase)
       {
          int localVarsAreLowerCamelCase = 0;

          // On failure, prefer returning an error-code 
          // (e.g. B_ERROR) over throwing an exception
          if (SomethingWentWrong()) return B_ERROR("Something bad happened");

          [...]

          return B_NO_ERROR; // returning B_NO_ERROR indicates success!
       }

       enum {
          COMPILE_TIME_CONSTANTS_ARE_ALL_UPPER_CASE
       };           

    private:
       int _memberVariablesStartWithAnUnderbar;
    };

    extern int _staticAndGlobalVariablesAlsoStartWithAnUnderbar;

    };  // end namespace muscle

    #endif
```
