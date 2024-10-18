cmake_minimum_required(VERSION 3.25)
project(electricity)

set(CMAKE_CXX_STANDARD 23)

find_package(Boost COMPONENTS algorithm date_time program_options REQUIRED)

add_executable(electricity main.cpp)
target_link_libraries(electricity PRIVATE Boost::program_options)
