cmake_minimum_required (VERSION 3.1)
project (muscle)
set (CMAKE_CXX_STANDARD 11)

file(GLOB MUSCLE_SRCS 
   "./dataio/*.cpp"
   "./iogateway/*.cpp"
   "./message/*.cpp"
   "./reflector/*.cpp"
   "./regex/*.cpp"
   "./syslog/*.cpp"
   "./system/*.cpp"
   "./util/*.cpp"
   "./zlib/*.cpp"
)

file(GLOB EXCLUDE_THESE_SOURCES
  "./dataio/*SSL*"
)
list(REMOVE_ITEM MUSCLE_SRCS ${EXCLUDE_THESE_SOURCES} )

if (WIN32)
   file(GLOB ZLIB_SRCS "./zlib/zlib/*.c")
   set(MUSCLE_SRCS 
      ${MUSCLE_SRCS}
      ${ZLIB_SRCS}
      "./regex/regex/regcomp.c"
      "./regex/regex/regerror.c"
      "./regex/regex/regexec.c"
      "./regex/regex/regfree.c"
   )
endif (WIN32)

add_library(muscle ${MUSCLE_SRCS})
target_include_directories(muscle PUBLIC .)
target_compile_definitions(muscle PUBLIC MUSCLE_ENABLE_ZLIB_ENCODING MUSCLE_NO_EXCEPTIONS)
if (APPLE)
   target_link_libraries(muscle "-framework CoreFoundation")
   target_link_libraries(muscle "-framework SystemConfiguration")
endif (APPLE)

if (WIN32)
   target_include_directories(muscle PRIVATE "./regex/regex" "./zlib/zlib")
else (!WIN32)
   target_link_libraries(muscle z)
endif (WIN32)

add_executable(muscled server/muscled.cpp)
target_include_directories(muscled PUBLIC .)
target_link_libraries(muscled muscle)
