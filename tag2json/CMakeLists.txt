cmake_minimum_required(VERSION 3.5)

project(tag2json LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(tag2json main.cpp tag_export.h tag_export.cpp)
