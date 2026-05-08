cmake_minimum_required(VERSION 3.20)

project(LogViewDll VERSION 1.0.0 LANGUAGES CXX)

add_library(LogView SHARED
    include/LogView.h
    src/LogView.cpp
)

target_compile_features(LogView PRIVATE cxx_std_17)
target_include_directories(LogView PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_definitions(LogView PRIVATE
    LOGVIEW_BUILD
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

if(MSVC)
    target_compile_options(LogView PRIVATE /W4 /permissive- /utf-8)
endif()

target_link_libraries(LogView PRIVATE
    d2d1
    dwrite
    user32
    gdi32
    ole32
)

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(LOGVIEW_ARCH_NAME "x86")
    target_sources(LogView PRIVATE exports/LogView_x86.def)
else()
    set(LOGVIEW_ARCH_NAME "x64")
    target_sources(LogView PRIVATE exports/LogView_x64.def)
endif()

set_target_properties(LogView PROPERTIES
    OUTPUT_NAME "LogView_${LOGVIEW_ARCH_NAME}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../dist/${LOGVIEW_ARCH_NAME}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../dist/${LOGVIEW_ARCH_NAME}"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../dist/${LOGVIEW_ARCH_NAME}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/../dist/${LOGVIEW_ARCH_NAME}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../dist/${LOGVIEW_ARCH_NAME}"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/../dist/${LOGVIEW_ARCH_NAME}"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../dist/${LOGVIEW_ARCH_NAME}"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/../dist/${LOGVIEW_ARCH_NAME}"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../dist/${LOGVIEW_ARCH_NAME}"
)

add_custom_command(TARGET LogView POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/../dist/include"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/include/LogView.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../dist/include/LogView.h"
)

add_executable(LogViewSmoke tests/smoke.cpp)
target_compile_features(LogViewSmoke PRIVATE cxx_std_17)
target_include_directories(LogViewSmoke PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
add_dependencies(LogViewSmoke LogView)
target_link_libraries(LogViewSmoke PRIVATE user32)
target_compile_definitions(LogViewSmoke PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(LogViewSmoke PRIVATE /W4 /permissive- /utf-8)
endif()
set_target_properties(LogViewSmoke PROPERTIES
    OUTPUT_NAME "LogViewSmoke_${LOGVIEW_ARCH_NAME}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../dist/${LOGVIEW_ARCH_NAME}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/../dist/${LOGVIEW_ARCH_NAME}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../dist/${LOGVIEW_ARCH_NAME}"
)
