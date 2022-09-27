cmake_minimum_required(VERSION 3.21)
include_guard(DIRECTORY)

message(STATUS "adding test: cparse-test")

project(cparse-test VERSION ${CPARSE_VERSION} LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Test
)

qt_add_executable(${PROJECT_NAME} cparse-test.cpp)
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Core Qt6::Test cparse)
