cmake_minimum_required(VERSION 3.18)
project(timeline-test)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sources, exe
set(_src_dir "${CMAKE_CURRENT_SOURCE_DIR}/src")
file(GLOB _source_list "${_src_dir}/*.cpp" "${_src_dir}/*.hpp")
set(_target "timeline-test")
add_executable(${_target} ${_source_list})

# Includes, libraries, compile options
find_package(Vulkan REQUIRED)
target_include_directories(${_target} PRIVATE ${_src_dir} ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${_target} PRIVATE ${Vulkan_LIBRARIES} )
