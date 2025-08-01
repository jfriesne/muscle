cmake_minimum_required(VERSION 3.10)

project(qtsupport)
if(NOT DEFINED CMAKE_CXX_STANDARD)
   set(CMAKE_CXX_STANDARD 11)
endif()
find_package(Threads REQUIRED)

find_package(Qt6Widgets QUIET)
if(Qt6Widgets_FOUND)
   # empty
else()
   find_package(Qt5Widgets REQUIRED)
endif()

set(CMAKE_AUTOMOC ON)
file(GLOB CODE_FILES *.cpp)
add_library(qtsupport ${CODE_FILES})
target_include_directories(qtsupport PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../..)

if(Qt6Widgets_FOUND)
   target_link_libraries(qtsupport Qt6::Widgets)
else()
   target_link_libraries(qtsupport Qt5::Widgets)
endif()

add_subdirectory(qt_muscled)
add_subdirectory(qt_muscled_browser)
add_subdirectory(qt_example)
add_subdirectory(qt_advanced_example)
