cmake_minimum_required (VERSION 2.8.12)

cmake_policy(SET CMP0020 NEW)

set(CMAKE_OBJECT_PATH_MAX 500)  # work-around for windows max-path-length lameness

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(TARGET muscle)
   return()  # This library has already been built
endif()

project (muscle)

option(WITH_THREADS "Enable CMake Threads support" ON)

option(WITH_CPLUSPLUS17 "Enable C++17 support" OFF)
if (WITH_CPLUSPLUS17)
   message("Note:  -DWITH_CPLUSPLUS17=ON argument was specified:  building MUSCLE with C++17 support enabled!")
   set (CMAKE_CXX_STANDARD 17)
   add_definitions(-DMUSCLE_USE_CPLUSPLUS17)
   if(${CMAKE_VERSION} VERSION_LESS "3.1.0")
      add_definitions(-std=c++17)   # fallback for older versions of CMake that don't know about CMAKE_CXX_STANDARD
   endif ()
   if (WIN32)
      add_definitions(-D_SILENCE_CXX17_IS_LITERAL_TYPE_DEPRECATION_WARNING)  # avoid getting spammed, I know
   endif()
else ()
   option(WITH_CPLUSPLUS11 "Enable C++11 support" ON)
   if (WITH_CPLUSPLUS11)
      set (CMAKE_CXX_STANDARD 11)
      if(${CMAKE_VERSION} VERSION_LESS "3.1.0")
         add_definitions(-std=c++11)   # fallback for older versions of CMake that don't know about CMAKE_CXX_STANDARD
      endif ()
   else ()
      message("Note:  -DWITH_CPLUSPLUS11=OFF argument was specified:  building MUSCLE without C++11 support!")
      add_definitions(-DMUSCLE_AVOID_CPLUSPLUS11)
      if (NOT WIN32 AND WITH_THREADS)
         add_definitions(-DMUSCLE_USE_PTHREADS)
      endif()
   endif (WITH_CPLUSPLUS11)
endif (WITH_CPLUSPLUS17)

option(WITH_PTHREADS "Enable pthreads support" OFF)
if (WITH_PTHREADS)
   message("Note:  -DWITH_PTHREADS=ON argument was specified:  building MUSCLE with pthreads support!")
   add_definitions(-DMUSCLE_USE_PTHREADS)
endif (WITH_PTHREADS)

option(WITH_HELGRIND "Enable helgrind annotations for reference-counting" OFF)
if (WITH_HELGRIND)
   message("Note:  -DWITH_HELGRIND=ON argument was specified:  building MUSCLE with helgrind annotations support!")
   add_definitions(-DMUSCLE_ENABLE_HELGRIND_ANNOTATIONS)
endif (WITH_HELGRIND)

option(WITH_SANITIZE "Enable building with clang's sanitizer-checks enabled" OFF)
if (WITH_SANITIZE)
   message("Note:  -DWITH_SANITIZE=ON argument was specified:  clang will build MUSCLE with sanitizer-checks enabled.")
   set(MUSCLE_SANITIZE_ARG "-fsanitize=address,undefined")
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MUSCLE_SANITIZE_ARG}")
   set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${MUSCLE_SANITIZE_ARG}")
endif (WITH_SANITIZE)

option(WITH_THREAD_SANITIZER "Enable Thread Sanitizer" OFF)
if (WITH_THREAD_SANITIZER)
   message("Note:   -DWITH_THREAD_SANITIZER=ON argument was specified:  Enabling Clang's Thread Sanitizer")
   add_compile_options(-fsanitize=thread -g)
   add_link_options(-fsanitize=thread)
endif()

option(WITH_TAGGED_POINTERS "Enable Tagged Pointers" ON)
if (WITH_THREAD_SANITIZER)
   # empty
else()
   message("Note:   -DWITH_TAGGED_POINTERS=OFF argument was specified:  PointerAndBits class will use separate fields instead")
   add_definitions(-DMUSCLE_AVOID_TAGGED_POINTERS)
endif()

option(WITH_DEADLOCKFINDER "Enable building with MUSCLE's potential-deadlock-detection logic enabled" OFF)
if (WITH_DEADLOCKFINDER)
   message("Note:  -DWITH_DEADLOCKFINDER=ON argument was specified:  MUSCLE will include expensive run-time debug-checks for potential deadlock conditions when locking Mutexes.")
   add_definitions(-DMUSCLE_ENABLE_DEADLOCK_FINDER)
endif (WITH_DEADLOCKFINDER)

option(WITH_IPV6 "Enable IPv6 support" ON)
if (NOT WITH_IPV6)
   message("Note:  -DWITH_IPV6=OFF argument was specified:  building MUSCLE without IPv6 support!")
   add_definitions(-DMUSCLE_AVOID_IPV6)
endif (NOT WITH_IPV6)

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
   "./zlib/zlib/contrib/minizip/*.c"
)

option(WITH_SSL "Enable OpenSSL Support" OFF)
if (WITH_SSL)
   message("Note:  -DWITH_SSL=ON argument was specified:  enabling OpenSSL support in MUSCLE.")
   find_package(OpenSSL REQUIRED)
   add_definitions(-DMUSCLE_ENABLE_SSL)
else ()
   message("Note:  -DWITH_SSL=ON argument was NOT specified:  not including OpenSSL support in MUSCLE.")
   file(GLOB EXCLUDE_THESE_SOURCES
     "./dataio/*SSL*"
   )
   list(REMOVE_ITEM MUSCLE_SRCS ${EXCLUDE_THESE_SOURCES} )
endif (WITH_SSL)

find_package(Threads REQUIRED)

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
   file(GLOB EXCLUDE_WIN32_SPECIFIC_SOURCES
      "./zlib/zlib/contrib/minizip/iowin32.c"
   )
   list(REMOVE_ITEM MUSCLE_SRCS ${EXCLUDE_WIN32_SPECIFIC_SOURCES})
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

if (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "iOS")
   add_library(muscle ${MUSCLE_SRCS})
else ()
   add_library(muscle STATIC ${MUSCLE_SRCS})
   target_compile_options(muscle PUBLIC -fembed-bitcode)
   target_link_libraries(muscle -fembed-bitcode)
   set_target_properties(muscle PROPERTIES
        FRAMEWORK TRUE
        MACOSX_FRAMEWORK_IDENTIFIER com.meyersound.muscle
   )
   install(TARGETS muscle
      FRAMEWORK DESTINATION lib
   )
   install(DIRECTORY "./dataio"
            DESTINATION lib/muscle.framework/Headers
            COMPONENT header_files
            FILES_MATCHING PATTERN "*.h"
            PATTERN "*SSL*" EXCLUDE
   )
   install(DIRECTORY "./iogateway"
            DESTINATION lib/muscle.framework/Headers
            COMPONENT header_files
            FILES_MATCHING PATTERN "*.h"
   )
   install(DIRECTORY "./message"
            DESTINATION lib/muscle.framework/Headers
            COMPONENT header_files
            FILES_MATCHING PATTERN "*.h"
   )
   install(DIRECTORY "./reflector"
            DESTINATION lib/muscle.framework/Headers
            COMPONENT header_files
            FILES_MATCHING PATTERN "*.h"
   )
   install(DIRECTORY "./regex"
            DESTINATION lib/muscle.framework/Headers
            COMPONENT header_files
            FILES_MATCHING PATTERN "*.h"
   )
   install(DIRECTORY "./support"
            DESTINATION lib/muscle.framework/Headers
            COMPONENT header_files
            FILES_MATCHING PATTERN "*.h"
   )
   install(DIRECTORY "./syslog"
            DESTINATION lib/muscle.framework/Headers
            COMPONENT header_files
            FILES_MATCHING PATTERN "*.h"
   )
   install(DIRECTORY "./system"
            DESTINATION lib/muscle.framework/Headers
            COMPONENT header_files
            FILES_MATCHING PATTERN "*.h"
   )
   install(DIRECTORY "./util"
            DESTINATION lib/muscle.framework/Headers
            COMPONENT header_files
            FILES_MATCHING PATTERN "*.h"
   )
   install(DIRECTORY "./zlib"
            DESTINATION lib/muscle.framework/Headers
            COMPONENT header_files
            FILES_MATCHING PATTERN "*.h"
   )
endif()

if (WITH_THREADS)
   target_link_libraries(muscle Threads::Threads)
else()
   message("Note:  -DWITH_THREADS=OFF argument was specified:  CMake will not attempt to link MUSCLE to a threads library.")
endif()

option(WITH_QT "Enable Qt5 Support" ON)
if (WITH_QT)
   find_package(Qt5Widgets)
   if(Qt5Widgets_FOUND)
      add_subdirectory(platform/qt)
   else()
      message("Note:  Qt5 wasn't found:  Qt executables and support code for MUSCLE will not be built")
   endif()
else()
   message("Note:  -DWITH_QT=OFF argument was specified:  Qt executables and support code for MUSCLE will not be built")
endif()

target_include_directories(muscle PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_compile_definitions(muscle PUBLIC MUSCLE_ENABLE_ZLIB_ENCODING MUSCLE_NO_EXCEPTIONS)
if (WITH_SSL)
   target_include_directories(muscle PRIVATE ${OPENSSL_INCLUDE_DIR})
   target_link_libraries(muscle OpenSSL::Crypto OpenSSL::SSL)
endif (WITH_SSL)

if (APPLE)
   find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
   if (NOT COREFOUNDATION_FRAMEWORK)
      message(FATAL_ERROR "CoreFoundation framework not found.")
   endif()
   target_link_libraries(muscle ${COREFOUNDATION_FRAMEWORK})
   if (NOT ${CMAKE_SYSTEM_NAME} STREQUAL iOS)
      find_library(SYSTEMCONFIGURATION_FRAMEWORK SystemConfiguration)
      if (NOT SYSTEMCONFIGURATION_FRAMEWORK)
         message(FATAL_ERROR "SystemConfiguration framework not found.")
      endif()
      target_link_libraries(muscle ${SYSTEMCONFIGURATION_FRAMEWORK})
      find_library(IOKIT_FRAMEWORK IOKit)
      if (NOT IOKIT_FRAMEWORK)
         message(FATAL_ERROR "IOKit framework not found.")
      endif()
      target_link_libraries(muscle ${IOKIT_FRAMEWORK})
   endif()
endif (APPLE)

if (WIN32)
   target_include_directories(muscle PRIVATE "./regex/regex" "./zlib/zlib")
else (!WIN32)
   target_link_libraries(muscle util)  # for forkpty()
   if (ZLIB_FOUND)
      target_link_libraries(muscle z)
   else (!ZLIB_FOUND)
      target_include_directories(muscle PRIVATE "./zlib/zlib")
      target_link_libraries(muscle)
   endif (ZLIB_FOUND)
endif (WIN32)

option(WITH_MUSCLED "Enable building of muscled" ON)
if (WITH_MUSCLED)
   add_executable(muscled server/muscled.cpp)
   target_include_directories(muscled PUBLIC .)
   target_link_libraries(muscled muscle)
else ()
   message("Note:  -DWITH_MUSCLED=OFF argument was specified:  muscled will not be built")
endif ()

option(WITH_TESTS "Enable building of muscle tests" OFF)
if (WITH_TESTS)
   message("Note:  -DWITH_TESTS=ON argument was specified:  programs in the tests folder will be built (type 'make all test' to run regression tests)")
   enable_testing()
   add_subdirectory(test)
endif (WITH_TESTS)

option(WITH_TOOLS "Enable building of muscle's miscellaneous tool executables" ON)
if (WITH_TOOLS)
   add_subdirectory(tools)
else ()
   message("Note:  -DWITH_TOOLS=OFF argument was specified:  programs in the tools folder will not be built")
endif (WITH_TOOLS)

option(WITH_EXAMPLES "Enable building of muscle examples" ON)
if (WITH_EXAMPLES)
   add_subdirectory(html/muscle-by-example/examples)
else()
   message("Note:  -DWITH_EXAMPLES=OFF argument was specified:  examples will not be built for MUSCLE")
endif (WITH_EXAMPLES)
