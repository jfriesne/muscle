#ifndef OVERLOADED_PREPROCESSOR_MACRO_H
#define OVERLOADED_PREPROCESSOR_MACRO_H

//-----------------------------------------------------------------------------
// OVERLOADED_PREPROCESSOR_MACRO
//
// used to create other macros with overloaded argument lists
//
// Example Use:
// #define myMacro(...) OVERLOADED_MACRO( myMacro, __VA_ARGS__ )
// #define myMacro0() someFunc()
// #define myMacro1( arg1 ) someFunc( arg1 )
// #define myMacro2( arg1, arg2 ) someFunc( arg1, arg2 )
//
// myMacro();
// myMacro(1);
// myMacro(1,2);
//
// Note the numerical suffix on the macro names,
// which indicates the number of arguments.
// That is the REQUIRED naming convention for your macros.
//
//-----------------------------------------------------------------------------

// OVERLOADED_PREPROCESSOR_MACRO
// derived from: https://stackoverflow.com/questions/11761703/overloading-macro-on-number-of-arguments

#define OVERLOADED_PREPROCESSOR_MACRO(M, ...) OPM_OVR(M, OPM_VA_NUM_ARGS(__VA_ARGS__)) (__VA_ARGS__)

#define OPM_OVR(macroName, number_of_args)   OPM_OVR_EXPAND(macroName, number_of_args)
#define OPM_OVR_EXPAND(macroName, number_of_args)    macroName##number_of_args
#define OPM_ARG_PATTERN_MATCH(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15, N, ...)   N
#define OPM_ARG16(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, ...) _15
#define OPM_HAS_COMMA(...) OPM_ARG16(__VA_ARGS__, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0)
#define OPM_HAS_NO_COMMA(...) OPM_ARG16(__VA_ARGS__, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1)
#define OPM_TRIGGER_PARENTHESIS_(...) ,

#define OPM_HAS_ZERO_OR_ONE_ARGS(...) \
    OPM_HAS_ZERO_OR_ONE_ARGS_AUX( \
    /* test if there is just one argument, eventually an empty one */ \
    OPM_HAS_COMMA(__VA_ARGS__), \
    /* test if OPM_TRIGGER_PARENTHESIS_ together with the argument adds a comma */ \
    OPM_HAS_COMMA(OPM_TRIGGER_PARENTHESIS_ __VA_ARGS__), \
    /* test if the argument together with a parenthesis adds a comma */ \
    OPM_HAS_COMMA(__VA_ARGS__ (~)), \
    /* test if placing it between _TRIGGER_PARENTHESIS_ and the parenthesis adds a comma */ \
    OPM_HAS_COMMA(OPM_TRIGGER_PARENTHESIS_ __VA_ARGS__ (~)) \
    )

#define OPM_PASTE5(_0, _1, _2, _3, _4) _0 ## _1 ## _2 ## _3 ## _4
#define OPM_HAS_ZERO_OR_ONE_ARGS_AUX(_0, _1, _2, _3) OPM_HAS_NO_COMMA(OPM_PASTE5(OPM_IS_EMPTY_CASE_, _0, _1, _2, _3))
#define OPM_IS_EMPTY_CASE_0001 ,

#define OPM_VA0(...) OPM_HAS_ZERO_OR_ONE_ARGS(__VA_ARGS__)
#define OPM_VA1(...) OPM_HAS_ZERO_OR_ONE_ARGS(__VA_ARGS__)
#define OPM_VA2(...) 2
#define OPM_VA3(...) 3
#define OPM_VA4(...) 4
#define OPM_VA5(...) 5
#define OPM_VA6(...) 6
#define OPM_VA7(...) 7
#define OPM_VA8(...) 8
#define OPM_VA9(...) 9
#define OPM_VA10(...) 10
#define OPM_VA11(...) 11
#define OPM_VA12(...) 12
#define OPM_VA13(...) 13
#define OPM_VA14(...) 14
#define OPM_VA15(...) 15
#define OPM_VA16(...) 16

#define OPM_VA_NUM_ARGS(...) OPM_VA_NUM_ARGS_IMPL(__VA_ARGS__, OPM_PP_RSEQ_N(__VA_ARGS__) )
#define OPM_VA_NUM_ARGS_IMPL(...) OPM_VA_NUM_ARGS_N(__VA_ARGS__)

#define OPM_VA_NUM_ARGS_N( \
    _1, _2, _3, _4, _5, _6, _7, _8, _9,_10, \
    _11,_12,_13,_14,_15,_16,N,...) N

#define OPM_PP_RSEQ_N(...) \
    OPM_VA16(__VA_ARGS__),OPM_VA15(__VA_ARGS__),OPM_VA14(__VA_ARGS__),OPM_VA13(__VA_ARGS__), \
    OPM_VA12(__VA_ARGS__),OPM_VA11(__VA_ARGS__),OPM_VA10(__VA_ARGS__), OPM_VA9(__VA_ARGS__), \
    OPM_VA8(__VA_ARGS__),OPM_VA7(__VA_ARGS__),OPM_VA6(__VA_ARGS__),OPM_VA5(__VA_ARGS__), \
    OPM_VA4(__VA_ARGS__),OPM_VA3(__VA_ARGS__),OPM_VA2(__VA_ARGS__),OPM_VA1(__VA_ARGS__), \
    OPM_VA0(__VA_ARGS__)

#endif
