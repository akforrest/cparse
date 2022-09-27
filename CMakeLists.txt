cmake_minimum_required(VERSION 3.21)
include_guard(DIRECTORY)

message(STATUS "adding library: cparse")
set(CPARSE_ENABLED TRUE PARENT_SCOPE)

set(CPARSE_VERSION "1.0.0" CACHE STRING "library version")
option(CPARSE_ENABLE_TESTING "Build unit tests" OFF)

project(cparse VERSION ${CPARSE_VERSION} LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS
    Core
)

list(APPEND ${PROJECT_NAME}_SOURCES
    config.cpp
    packtoken.cpp
    functions.cpp
    containers.cpp
    calculator.cpp
    reftoken.cpp
    rpnbuilder.cpp
    builtin-features/functions.h
    builtin-features/operations.h
    builtin-features/reservedwords.h
    builtin-features/typespecificfunctions.h
    include/cparse/config.h
    include/cparse/cparse.h
    include/cparse/operation.h
    include/cparse/packtoken.h
    include/cparse/functions.h
    include/cparse/containers.h
    include/cparse/calculator.h
    include/cparse/reftoken.h
    include/cparse/rpnbuilder.h
    include/cparse/token.h
    include/cparse/tokenhelpers.h
    include/cparse/tokentype.h
)

if(CPARSE_ENABLE_TESTING)
    add_subdirectory(test)
endif()

qt_add_library(${PROJECT_NAME} STATIC ${${PROJECT_NAME}_SOURCES})

target_compile_definitions(${PROJECT_NAME} INTERFACE CPARSE_ENABLE_CPARSE)
target_include_directories(${PROJECT_NAME} PUBLIC include)
target_include_directories(${PROJECT_NAME} PRIVATE include/cparse)
target_link_libraries(${PROJECT_NAME} PUBLIC Qt6::Core)
