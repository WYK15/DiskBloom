cmake_minimum_required(VERSION 3.25)

project(DiskBloom VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ".")
include(InstallRequiredSystemLibraries)

include(CTest)

add_subdirectory(src)

option(DISKBLOOM_BUILD_BENCHMARKS "Build DiskBloom performance benchmarks" ON)
if(DISKBLOOM_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_FILE_NAME "DiskBloom-${PROJECT_VERSION}-windows-x64")
set(CPACK_PACKAGE_VENDOR "DiskBloom Contributors")
include(CPack)
