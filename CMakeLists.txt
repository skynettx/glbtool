cmake_minimum_required(VERSION 3.10)

set(EXECUTABLE_OUTPUT_PATH "bin")
set(CMAKE_CXX_FLAGS "-O3")
SET(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_FLAGS "-O3")
SET(CMAKE_C_STANDARD 17)
set(CMAKE_BUILD_TYPE RelWithDebInfo)

project(glbtool)
include_directories(./src/)

add_executable(${CMAKE_PROJECT_NAME}
src/main.cpp
src/decrypt.cpp
src/encrypt.cpp
src/graphics.cpp
src/sounds.cpp
src/lodepng.cpp
)

target_link_libraries(glbtool)