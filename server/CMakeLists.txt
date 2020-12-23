cmake_minimum_required(VERSION 3.9)
project(web-alarm-server)

set(CMAKE_CXX_STANDARD 11)

find_package(Boost REQUIRED)
find_package(websocketpp REQUIRED)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} pthread)
