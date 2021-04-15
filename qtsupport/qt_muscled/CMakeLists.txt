file(GLOB_RECURSE CODE_FILES *.cpp)
add_executable(qt_muscled ${CODE_FILES} ../../server/muscled.cpp)
target_compile_definitions(qt_muscled PUBLIC 
    UNIFIED_DAEMON
    MUSCLE_ENABLE_ZLIB_ENCODING
    MUSCLE_SINGLE_THREAD_ONLY
    MUSCLE_AVOID_IPV6
    )
target_link_libraries(qt_muscled
  Qt5::Widgets
  qtsupport
  muscle
)

