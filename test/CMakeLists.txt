cmake_minimum_required(VERSION 3.13)
project(${PACKAGE_NAME}-test)

file(DOWNLOAD "https://github.com/catchorg/Catch2/releases/download/v2.5.0/catch.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/catch.hpp"
              EXPECTED_HASH SHA512=a6fa4ebfdef2f4a021122ba893d8cec543600698ded3ad2d4bc39d36ebec812570fa710271bffdeb02606d85c9c64056dcb426e076c7ca5db312b56537ba8fd8)

file(GLOB SRC_FILES *.cpp)

add_executable(${PROJECT_NAME} ${SRC_FILES})
target_link_libraries(${PROJECT_NAME} PUBLIC ${PACKAGE_NAME}::${PACKAGE_NAME})
