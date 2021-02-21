cmake_minimum_required(VERSION 3.10)
project(code)

set(CMAKE_CXX_STANDARD 11)

include_directories(.)

add_executable(code
        complex.desc
        complex.topo
        complex2.desc
        dr
        dr_api.c
        dr_api.h
        launch_dr.sh
        lvns
        lvns_types.h
        Makefile
        README
        rmutex.c
        rmutex.h
        simple.topo
        star.topo
        tri.topo
        update_binaries.sh)
