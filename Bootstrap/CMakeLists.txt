project(Bootstrap)

cmake_minimum_required(VERSION 3.21)

set (CMAKE_CXX_STANDARD 20)

set(CMAKE_C_ARCHIVE_FINISH   "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
set(CMAKE_CXX_ARCHIVE_FINISH "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")

add_subdirectory(Runtime)
