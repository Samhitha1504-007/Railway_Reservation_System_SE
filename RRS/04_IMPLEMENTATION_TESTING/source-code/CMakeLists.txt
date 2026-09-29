cmake_minimum_required(VERSION 3.16)
project(RRS C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(Curses REQUIRED)

file(GLOB_RECURSE RRS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

add_executable(rrs_app ${RRS_SOURCES})
target_include_directories(rrs_app PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    ${CURSES_INCLUDE_DIRS}
)
target_link_libraries(rrs_app PRIVATE ${CURSES_LIBRARIES})

enable_testing()
find_package(GTest REQUIRED)

add_executable(rrs_tests
    tests/test_auth.c
    tests/test_inventory.c
    tests/test_search.c
    tests/test_booking.c
)
target_include_directories(rrs_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
set_target_properties(rrs_tests PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(rrs_tests PRIVATE GTest::gtest_main)
add_test(NAME rrs_tests COMMAND rrs_tests)
