# Standalone helper, not part of the main build.
cmake_minimum_required(VERSION 3.20)
project(qsensors_icon_generator CXX)
set(CMAKE_CXX_STANDARD 20)
find_package(Qt6 REQUIRED COMPONENTS Gui)
add_executable(generate_icon generate_icon.cpp ../../src/ui/widgets/lcd_segment_font.cpp)
target_include_directories(generate_icon PRIVATE ../../src/ui/widgets)
target_link_libraries(generate_icon Qt6::Gui)
