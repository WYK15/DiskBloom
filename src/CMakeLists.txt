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

add_library(
    diskbloom_core
    STATIC
        core/scan_tree.cpp
        core/string_catalog.cpp
        core/theme.cpp
)
target_link_libraries(diskbloom_core PUBLIC diskbloom_options)
target_include_directories(diskbloom_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(
    diskbloom_platform_windows
    STATIC
        platform/windows/system_theme.cpp
        platform/windows/volume_service.cpp
)
target_link_libraries(
    diskbloom_platform_windows
    PUBLIC
        diskbloom_core
        diskbloom_options
    PRIVATE
        advapi32
)
target_include_directories(diskbloom_platform_windows PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(
    diskbloom_render
    STATIC
        render/graphics_device.cpp
)
target_link_libraries(
    diskbloom_render
    PUBLIC
        diskbloom_core
        diskbloom_options
    PRIVATE
        d2d1
        d3d11
        dwrite
        dxgi
)
target_include_directories(diskbloom_render PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(
    diskbloom_app
    STATIC
        app/appearance_settings.cpp
        app/disk_overview.cpp
        app/main_window.cpp
)
target_link_libraries(
    diskbloom_app
    PUBLIC
        diskbloom_core
        diskbloom_options
        diskbloom_platform_windows
        diskbloom_render
    PRIVATE
        dwmapi
        shell32
)
target_include_directories(diskbloom_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(DiskBloom WIN32 app/win_main.cpp)
target_link_libraries(DiskBloom PRIVATE diskbloom_app diskbloom_options shell32)
