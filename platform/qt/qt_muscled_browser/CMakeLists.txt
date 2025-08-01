cmake_minimum_required(VERSION 3.10)

if(NOT DEFINED CMAKE_CXX_STANDARD)
   set(CMAKE_CXX_STANDARD 11)
endif()
file(GLOB_RECURSE CODE_FILES *.cpp)
add_executable(qt_muscled_browser ${CODE_FILES} ../../../server/muscled.cpp)
target_compile_definitions(qt_muscled_browser PUBLIC
    UNIFIED_DAEMON
    MUSCLE_ENABLE_ZLIB_ENCODING
    MUSCLE_AVOID_IPV6
    )

find_package(Qt6Widgets QUIET)
if(Qt6Widgets_FOUND)
    SET(QtPrefix "Qt6")
else()
    find_package(Qt5Widgets REQUIRED)
    SET(QtPrefix "Qt5")
endif()

target_link_libraries(qt_muscled_browser
    ${QtPrefix}::Widgets
    Threads::Threads
    qtsupport
    muscle
    )
