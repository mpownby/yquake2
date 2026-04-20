cmake_minimum_required(VERSION 3.20)
project(bsp2rbx LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
# For Windows: prevent overriding parent project's compiler/linker settings.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

set(BSP2RBX_SOURCES
    include/bsp2rbx/FileReader.cpp
    include/bsp2rbx/FileWriter.cpp
    include/bsp2rbx/BspParser.cpp
    include/bsp2rbx/BrushGeometry.cpp
    include/bsp2rbx/SolidWorldspawnFilter.cpp
    include/bsp2rbx/RobloxXmlWriter.cpp
    include/bsp2rbx/BspConverter.cpp
)

add_library(bsp2rbx_lib OBJECT ${BSP2RBX_SOURCES})
target_include_directories(bsp2rbx_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/common/header
)

add_executable(bsp2rbx src/main.cpp $<TARGET_OBJECTS:bsp2rbx_lib>)
target_link_libraries(bsp2rbx PRIVATE bsp2rbx_lib)

enable_testing()
include(GoogleTest)

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/tests/*Test.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e/*Test.cpp)

foreach(src ${TEST_SOURCES})
    get_filename_component(name ${src} NAME_WE)
    add_executable(${name} ${src} $<TARGET_OBJECTS:bsp2rbx_lib>)
    target_include_directories(${name}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/src/common/header
    )
    target_link_libraries(${name} PRIVATE bsp2rbx_lib gmock_main)
    gtest_discover_tests(${name})
endforeach()
