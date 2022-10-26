cmake_minimum_required(VERSION 3.5)

project(GO_attack)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(Boost COMPONENTS system)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -g -Wall -mpopcnt -pthread")

add_executable(GO_attack main.cpp)
target_link_libraries(GO_attack ${Boost_SYSTEM_LIBRARY})

