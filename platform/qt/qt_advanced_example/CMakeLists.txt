if(NOT DEFINED CMAKE_CXX_STANDARD)
   set(CMAKE_CXX_STANDARD 11)
endif()
file(GLOB_RECURSE CODE_FILES *.cpp)
add_executable(qt_advanced_example ${CODE_FILES} ../../../server/muscled.cpp)
target_compile_definitions(qt_advanced_example PUBLIC 
    UNIFIED_DAEMON
    MUSCLE_ENABLE_ZLIB_ENCODING
    MUSCLE_AVOID_IPV6
    )
target_link_libraries(qt_advanced_example
  Qt5::Widgets
  Threads::Threads
  qtsupport
  muscle
)

