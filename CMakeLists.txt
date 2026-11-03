cmake_minimum_required(VERSION 3.16)
project(GomokuGame LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(GomokuGame
    main.cpp
    gomoku.cpp
    gomoku.h
)

target_link_libraries(GomokuGame PRIVATE Qt6::Widgets)
target_compile_features(GomokuGame PRIVATE cxx_std_17)
