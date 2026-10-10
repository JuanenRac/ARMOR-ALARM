# ARMOR-ALARM host tests: the zones and the rules of the panel, built with any C++17 compiler.
# Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
cmake_minimum_required(VERSION 3.16)
project(armor_alarm_host_tests CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic -Werror)

add_executable(test_alarm test_alarm.cpp)

enable_testing()
add_test(NAME alarm COMMAND test_alarm)
