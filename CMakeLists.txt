cmake_minimum_required(VERSION 3.2)
project(nanoarq)

set(CMAKE_CXX_STANDARD 11)
include(ExternalProject)

################ CppUTest

set(CPPUTEST_ROOT_DIR ${CMAKE_SOURCE_DIR}/external/CppUTest)
set(CPPUTEST_LIB_DIR ${CPPUTEST_ROOT_DIR}/bin/lib)
set(CPPUTEST_INCLUDE_DIR ${CPPUTEST_ROOT_DIR}/bin/include)

ExternalProject_Add(CppUTest_external
                    PREFIX ${CPPUTEST_ROOT_DIR}
                    GIT_REPOSITORY "https://github.com/cpputest/cpputest.git"
                    GIT_TAG "f09a20eab1510357514f0f655253537f2dcbc229"
                    UPDATE_COMMAND ""
                    PATCH_COMMAND ""
                    BINARY_DIR ${CPPUTEST_ROOT_DIR}/src/CppUTest
                    SOURCE_DIR ${CPPUTEST_ROOT_DIR}/src/CppUTest
                    INSTALL_DIR ${CPPUTEST_ROOT_DIR}/bin
                    CONFIGURE_COMMAND ./configure --prefix=<INSTALL_DIR>
                    BUILD_COMMAND make
                    BUILD_BYPRODUCTS ${CPPUTEST_LIB_DIR}/libCppUTest.a
                                     ${CPPUTEST_LIB_DIR}/libCppUTestExt.a)

ExternalProject_Add_Step(CppUTest_external
                         autogen
                         COMMAND ./autogen.sh
                         DEPENDEES download
                         DEPENDERS configure
                         WORKING_DIRECTORY ${CPPUTEST_ROOT_DIR}/src/CppUTest)

add_library(libCppUTest STATIC IMPORTED)
set_target_properties(libCppUTest PROPERTIES IMPORTED_LOCATION ${CPPUTEST_LIB_DIR}/libCppUTest.a)

add_library(libCppUTestExt STATIC IMPORTED)
set_target_properties(libCppUTestExt PROPERTIES IMPORTED_LOCATION ${CPPUTEST_LIB_DIR}/libCppUTestExt.a)

add_dependencies(libCppUTest CppUTest_external)
add_dependencies(libCppUTestExt CppUTest_external)

################ plthook

set(PLTHOOK_ROOT_DIR ${CMAKE_SOURCE_DIR}/external/plthook)
set(PLTHOOK_SRC_DIR ${PLTHOOK_ROOT_DIR}/bin)
set(PLTHOOK_INCLUDE_DIR ${PLTHOOK_SRC_DIR})

ExternalProject_Add(plthook_external
                    PREFIX ${PLTHOOK_ROOT_DIR}
                    GIT_REPOSITORY "https://github.com/kubo/plthook.git"
                    GIT_TAG "c731d97f1b81b9c4fd26279fc09112d3f8c7b82a"
                    UPDATE_COMMAND ""
                    PATCH_COMMAND ""
                    CONFIGURE_COMMAND ""
                    BUILD_COMMAND ""
                    INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_directory ${PLTHOOK_ROOT_DIR}/src/plthook_external ${PLTHOOK_SRC_DIR}
                    BUILD_BYPRODUCTS ${PLTHOOK_SRC_DIR}/plthook_osx.c
                                     ${PLTHOOK_SRC_DIR}/plthook_elf.c
                                     ${PLTHOOK_SRC_DIR}/plthook_win32.c
                                     ${PLTHOOK_SRC_DIR}/plthook.h)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(PLTHOOK_SOURCES ${PLTHOOK_SRC_DIR}/plthook_osx.c)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(PLTHOOK_SOURCES ${PLTHOOK_SRC_DIR}/plthook_elf.c)
else()
    error("Unsupported platform ${CMAKE_SYSTEM_NAME}")
endif()

add_library(plthook STATIC ${PLTHOOK_SOURCES})
add_dependencies(plthook plthook_external)

################ tests

include_directories(${CPPUTEST_INCLUDE_DIR} ${PLTHOOK_INCLUDE_DIR})
add_executable(nanoarq_tests unit_tests/main.cpp)
target_link_libraries(nanoarq_tests libCppUTest libCppUTestExt plthook dl)

set(NANOARQ_TESTS_TIMESTAMP_FILE ${CMAKE_CURRENT_BINARY_DIR}/nanoarq_tests.timestamp)
add_custom_target(RUN_NANOARQ_TESTS ALL DEPENDS ${NANOARQ_TESTS_TIMESTAMP_FILE})
add_custom_command(OUTPUT ${NANOARQ_TESTS_TIMESTAMP_FILE}
                   COMMAND nanoarq_tests && touch ${NANOARQ_TESTS_TIMESTAMP_FILE}
                   DEPENDS nanoarq_tests
                   COMMENT "Running nanoarq_tests")
