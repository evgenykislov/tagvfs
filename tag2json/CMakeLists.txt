cmake_minimum_required(VERSION 3.5)

project(tag2json LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(HEADER_FILES
  "../libs/json.hpp"
  "tag_export.h"
  "tag_file.h"
  "formats/tag_file_43235634.h")

set(SOURCE_FILES
  "main.cpp"
  "tag_export.cpp"
  "tag_file.cpp"
  "formats/tag_file_43235634.cpp")


add_executable(tag2json ${HEADER_FILES} ${SOURCE_FILES})
