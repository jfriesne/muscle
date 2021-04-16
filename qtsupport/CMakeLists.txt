project(qtsupport)
find_package(Threads REQUIRED)
find_package(Qt5Widgets REQUIRED)
set(CMAKE_AUTOMOC ON)
file(GLOB CODE_FILES *.cpp)
add_library(qtsupport ${CODE_FILES})
target_include_directories(qtsupport PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(qtsupport
  Qt5::Widgets
)
add_subdirectory(qt_muscled)
add_subdirectory(qt_muscled_browser)
add_subdirectory(qt_example)
add_subdirectory(qt_advanced_example)
