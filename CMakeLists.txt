cmake_minimum_required(VERSION 3.25)

project(DiskBloom VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

