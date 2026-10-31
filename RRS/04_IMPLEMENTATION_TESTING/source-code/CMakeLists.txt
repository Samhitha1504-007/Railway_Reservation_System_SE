cmake_minimum_required(VERSION 3.16)
project(RRS C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Curses REQUIRED)
find_package(OpenSSL REQUIRED)

file(GLOB_RECURSE RRS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)
# Modules with their own library target are linked in, not compiled twice.
list(FILTER RRS_SOURCES EXCLUDE REGEX "/src/inventory/")

# Auth module (C++17, OpenSSL for password hashing). Public header: include/auth.h
add_library(rrs_auth STATIC
    src/auth/auth.cpp
    src/auth/password.cpp
    src/auth/session.cpp
    src/auth/rbac.cpp
)
target_include_directories(rrs_auth
    PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/auth"
)
target_link_libraries(rrs_auth PUBLIC OpenSSL::Crypto)

# Search module (C++17). Public header: include/search.h
add_library(rrs_search STATIC
    src/search/graph.cpp
    src/search/search.cpp
)
target_include_directories(rrs_search
    PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/search"
)

# Inventory module (C11). Public header: include/inventory.h
add_library(rrs_inventory STATIC
    src/inventory/inventory.c
)
target_include_directories(rrs_inventory
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(rrs_inventory PUBLIC rrs_search PRIVATE rrs_auth)

add_executable(rrs_app ${RRS_SOURCES})
target_include_directories(rrs_app PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    ${CURSES_INCLUDE_DIRS}
)
target_link_libraries(rrs_app PRIVATE ${CURSES_LIBRARIES} rrs_auth rrs_search rrs_inventory)

enable_testing()
find_package(GTest REQUIRED)

add_executable(rrs_tests
    tests/test_booking.c
)
target_include_directories(rrs_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
set_target_properties(rrs_tests PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(rrs_tests PRIVATE GTest::gtest_main)
add_test(NAME rrs_tests COMMAND rrs_tests)

add_executable(rrs_inventory_tests tests/test_inventory.cpp)
target_include_directories(rrs_inventory_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/auth"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/inventory"
)
target_link_libraries(rrs_inventory_tests PRIVATE rrs_inventory rrs_auth GTest::gtest_main)
add_test(NAME rrs_inventory_tests COMMAND rrs_inventory_tests)

add_executable(rrs_auth_tests tests/test_auth.cpp)
target_include_directories(rrs_auth_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/auth"
)
target_link_libraries(rrs_auth_tests PRIVATE rrs_auth GTest::gtest_main)
add_test(NAME rrs_auth_tests COMMAND rrs_auth_tests)

add_executable(rrs_search_tests tests/test_search.cpp)
target_link_libraries(rrs_search_tests PRIVATE rrs_search GTest::gtest_main)
add_test(NAME rrs_search_tests COMMAND rrs_search_tests)
