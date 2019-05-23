cmake_minimum_required (VERSION 2.8)

if(TARGET muscle)
   return()  # This library has already been built
endif()

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
else (!WIN32)
   if(${CMAKE_VERSION} VERSION_LESS "3.1.0")
      add_definitions(-std=c++11)   # fallback for older versions of CMake that don't know about CMAKE_CXX_STANDARD
   endif()
   find_package(ZLIB 1.2.8)
   if (ZLIB_FOUND)
      # Cool, we'll use the system-provided zlib
   else (!ZLIB_FOUND)
      message("No Zlib library installed on system: falling back to using MUSCLE's captive zlib implementation instead")
      file(GLOB ZLIB_SRCS "./zlib/zlib/*.c")
      set(MUSCLE_SRCS
         ${MUSCLE_SRCS}
         ${ZLIB_SRCS})
   endif (ZLIB_FOUND)
endif (WIN32)

add_library(muscle ${MUSCLE_SRCS})
target_include_directories(muscle PUBLIC .)
target_compile_definitions(muscle PUBLIC MUSCLE_ENABLE_ZLIB_ENCODING MUSCLE_NO_EXCEPTIONS)
if (APPLE)
   target_link_libraries(muscle "-framework CoreFoundation")
   target_link_libraries(muscle "-framework SystemConfiguration")
   target_link_libraries(muscle "-framework IOKit")
endif (APPLE)

if (WIN32)
   target_include_directories(muscle PRIVATE "./regex/regex" "./zlib/zlib")
else (!WIN32)
   if (ZLIB_FOUND)
      target_link_libraries(muscle z)
   else (!ZLIB_FOUND)
      target_include_directories(muscle PRIVATE "./zlib/zlib")
      target_link_libraries(muscle)
   endif (ZLIB_FOUND)
endif (WIN32)

add_executable(muscled server/muscled.cpp)
target_include_directories(muscled PUBLIC .)
target_link_libraries(muscled muscle)
