cmake_minimum_required(VERSION 3.25)

project(DiskBloom VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

add_subdirectory(src)

option(DISKBLOOM_BUILD_BENCHMARKS "Build DiskBloom performance benchmarks" ON)
if(DISKBLOOM_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
