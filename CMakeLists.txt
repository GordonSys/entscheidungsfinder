﻿cmake_minimum_required (VERSION 3.8)

project ("entscheidungsfinder")
set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED True)

find_package (Threads)

add_compile_definitions (_CRT_SECURE_NO_WARNINGS)
add_executable (entscheidungsfinder "entscheidungsfinder.cpp")
target_link_libraries (entscheidungsfinder ${CMAKE_THREAD_LIBS_INIT})