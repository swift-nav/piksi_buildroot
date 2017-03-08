cmake_minimum_required(VERSION 2.8.10)

project(skylark C)

include ( FindCURL )

add_definitions(-std=gnu11)

add_library(skylark libskylark.c libskylark.h)
