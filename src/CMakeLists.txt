add_library(diskbloom_options INTERFACE)

target_compile_features(diskbloom_options INTERFACE cxx_std_20)
target_compile_definitions(
    diskbloom_options
    INTERFACE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
)

if(MSVC)
    target_compile_options(
        diskbloom_options
        INTERFACE
            /W4
            /permissive-
            /utf-8
            /EHsc
    )
endif()

