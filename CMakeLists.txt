cmake_minimum_required (VERSION 2.8)

cmake_policy(SET CMP0020 NEW)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

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
   "./zlib/zlib/contrib/minizip/*.c"
)

file(GLOB EXCLUDE_THESE_SOURCES
  "./dataio/*SSL*"
)
list(REMOVE_ITEM MUSCLE_SRCS ${EXCLUDE_THESE_SOURCES} )

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

target_link_libraries(muscle Threads::Threads)

option(WITH_QT "Enable Qt5 Support" ON)
if (WITH_QT)
	find_package(Qt5Widgets)
	if(Qt5Widgets_FOUND)
		add_subdirectory(qtsupport)
	else()
   		message("Note:  Qt5 wasn't found:  Qt executables and support code will not be built")
	endif()
else()
   message("Note:  -DWITH_QT=OFF argument was specified:  Qt executables and support code will not be built")
endif()

target_include_directories(muscle PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_compile_definitions(muscle PUBLIC MUSCLE_ENABLE_ZLIB_ENCODING MUSCLE_NO_EXCEPTIONS)
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

option(WITH_TESTS "Enable building of muscle tests" ON)
if (WITH_TESTS)
   add_subdirectory(test)
else()
   message("Note:  -DWITH_TESTS=OFF argument was specified:  tests will not be built")
endif (WITH_TESTS)

option(WITH_EXAMPLES "Enable building of muscle examples" ON)
if (WITH_EXAMPLES)
   add_subdirectory(html/muscle-by-example/examples)
else()
   message("Note:  -DWITH_EXAMPLES=OFF argument was specified:  examples will not be built")
endif (WITH_EXAMPLES)
