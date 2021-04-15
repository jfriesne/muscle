file(GLOB_RECURSE CODE_FILES *.cpp)
add_executable(qt_example ${CODE_FILES} ../../server/muscled.cpp)
target_compile_definitions(qt_example PUBLIC 
    UNIFIED_DAEMON
    MUSCLE_ENABLE_ZLIB_ENCODING
    MUSCLE_AVOID_IPV6
    )
target_link_libraries(qt_example
  Qt5::Widgets
  Threads::Threads
  qtsupport
  muscle
)

