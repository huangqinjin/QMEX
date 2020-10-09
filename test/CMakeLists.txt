cmake_minimum_required(VERSION 3.18)
project(${PACKAGE_NAME}-test)

file(DOWNLOAD "https://github.com/catchorg/Catch2/releases/download/v2.13.2/catch.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/catch.hpp"
              EXPECTED_HASH SHA512=025f8a443dc681acdea56ac067083b52c8c0c85fc7b795a7781d04c016ddfe0fb25570d002e29e4ff4b3a9d2b9036e9b0e356cd2f46fabef2805a5bda962385a)

file(GLOB SRC_FILES *.cpp)

add_executable(${PROJECT_NAME} ${SRC_FILES})
target_link_libraries(${PROJECT_NAME} PUBLIC ${PACKAGE_NAME}::${PACKAGE_NAME})
