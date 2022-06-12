cmake_minimum_required(VERSION 3.21)
project(${PACKAGE_NAME}-test)

file(DOWNLOAD "https://github.com/catchorg/Catch2/releases/download/v2.13.9/catch.hpp"
              "${CMAKE_CURRENT_LIST_DIR}/catch.hpp"
              EXPECTED_HASH SHA512=3ff722c4c85ff16e6121705c1a5b97f51fabab13dc8594efc213d81bbc485dc4af7bec1ffbe55a732b363e605e9a4c0703e28c975e3b3ea676e2889fedd01b43)

file(GLOB SRC_FILES *.cpp)

add_executable(${PROJECT_NAME} ${SRC_FILES})
target_link_libraries(${PROJECT_NAME} PUBLIC ${PACKAGE_NAME}::${PACKAGE_NAME})


add_test(NAME "Unit Tests" COMMAND ${PROJECT_NAME})

add_test(NAME "Lua Demo" COMMAND ${PACKAGE_NAME}::cli demo.lua WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})

find_program(CMD cmd.exe)
if(CMD)
    add_test(NAME "Bat Demo" WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
             COMMAND demo.bat $<SHELL_PATH:$<TARGET_FILE:${PACKAGE_NAME}::cli>>)
endif()

find_program(BASH bash)
if(BASH)
    add_test(NAME "Bash Demo" WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
             COMMAND ${BASH} demo.sh $<TARGET_FILE:${PACKAGE_NAME}::cli>)
endif()

