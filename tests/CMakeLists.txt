# ARMOR-ALARM host tests: the zones, the rules of the panel, the node's settings and messages and the runner that ties them together, built with any C++17 compiler.
# Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
cmake_minimum_required(VERSION 3.16)
project(armor_alarm_host_tests CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic -Werror)

add_executable(test_alarm test_alarm.cpp)
add_executable(test_node test_node.cpp)
add_executable(test_release test_release.cpp)
add_executable(test_zigbee test_zigbee.cpp)
add_executable(emit_samples emit_samples.cpp)

enable_testing()
add_test(NAME alarm COMMAND test_alarm)
add_test(NAME node COMMAND test_node)
add_test(NAME release COMMAND test_release)
add_test(NAME zigbee COMMAND test_zigbee)
