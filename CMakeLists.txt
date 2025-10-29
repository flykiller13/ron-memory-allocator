cmake_minimum_required(VERSION 4.0)
project(ron_memory_allocator C)

set(CMAKE_C_STANDARD 11)

add_executable(fixed_size_allocatoron ron-memory-allocator/fixed_size_allocatoron.c)
add_executable(variable_size_allocatoron ron-memory-allocator/variable_size_allocatoron.c)
