cmake_minimum_required(VERSION 3.23)

project(qmex)
set(PACKAGE_NAME ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_library(${PROJECT_NAME} qmex.cpp)
add_library(${PACKAGE_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
target_sources(${PROJECT_NAME} PUBLIC FILE_SET HEADERS FILES qmex.hpp)

find_package(Lua REQUIRED)
target_include_directories(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${LUA_INCLUDE_DIR}>)
target_link_libraries(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${LUA_LIBRARIES}>)

get_target_property(PROJECT_LIBRARY_TYPE ${PROJECT_NAME} TYPE)
string(TOUPPER ${PROJECT_NAME} PROJECT_UPPER_NAME)
string(MAKE_C_IDENTIFIER ${PROJECT_UPPER_NAME} PROJECT_UPPER_NAME)
target_compile_definitions(${PROJECT_NAME} INTERFACE ${PROJECT_UPPER_NAME}_${PROJECT_LIBRARY_TYPE})
if(PROJECT_LIBRARY_TYPE STREQUAL "SHARED_LIBRARY")
    if(WIN32)
        # https://github.com/lua/lua/blob/v5.4.0/luaconf.h#L222
        set(suffix .dll)
    else()
        # https://github.com/lua/lua/blob/v5.4.0/luaconf.h#L242
        set(suffix .so)
    endif()
    set_target_properties(${PROJECT_NAME} PROPERTIES
        PREFIX "" SUFFIX ${suffix}
        DEFINE_SYMBOL ${PROJECT_UPPER_NAME}_EXPORTS
    )
endif()

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME} EXPORT ${PACKAGE_NAME}-config FILE_SET HEADERS)

option(BUILD_TOOLS "Build cli tools" ON)
if(BUILD_TOOLS)
    project(${PACKAGE_NAME}-cli)
    add_executable(${PROJECT_NAME} cli.cpp)
    add_executable(${PACKAGE_NAME}::cli ALIAS ${PROJECT_NAME})
    target_link_libraries(${PROJECT_NAME} PUBLIC ${PACKAGE_NAME}::${PACKAGE_NAME})
    install(TARGETS ${PROJECT_NAME} EXPORT ${PACKAGE_NAME}-config)
endif()

option(BUILD_TESTING "Build the testing tree." ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(test)
endif()

install(EXPORT ${PACKAGE_NAME}-config
    NAMESPACE ${PACKAGE_NAME}::
    DESTINATION ${CMAKE_INSTALL_DATADIR}/${PACKAGE_NAME}
)
