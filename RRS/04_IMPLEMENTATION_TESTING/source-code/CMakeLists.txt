cmake_minimum_required(VERSION 3.16)
project(RRS C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Curses REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

file(GLOB_RECURSE RRS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)
# Modules with their own library target are linked in, not compiled twice.
list(FILTER RRS_SOURCES EXCLUDE REGEX "/src/inventory/")
list(FILTER RRS_SOURCES EXCLUDE REGEX "/src/booking/")

# Audit logger (SEC-006). Public header: include/audit.h
add_library(rrs_audit STATIC src/audit/audit.cpp)
target_include_directories(rrs_audit
    PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/audit"
)

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
target_link_libraries(rrs_auth PUBLIC OpenSSL::Crypto rrs_audit)

# Search module (C++17). Public header: include/search.h
add_library(rrs_search STATIC
    src/search/graph.cpp
    src/search/search.cpp
    src/search/fare_eco.cpp
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

# Console walkthrough of the Sprint 1 features (no TUI needed).
add_executable(rrs_demo src/demo/rrs_demo.cpp)
target_include_directories(rrs_demo PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/auth")
target_link_libraries(rrs_demo PRIVATE rrs_auth rrs_inventory rrs_search rrs_booking)

# Booking module (C11). Public header: include/booking.h
add_library(rrs_booking STATIC
    src/booking/booking.c
    src/booking/waitlist.c
)
target_include_directories(rrs_booking
    PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/booking"
)
target_link_libraries(rrs_booking PUBLIC rrs_inventory rrs_auth Threads::Threads)

add_executable(rrs_app ${RRS_SOURCES})
target_include_directories(rrs_app PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    ${CURSES_INCLUDE_DIRS}
)
target_link_libraries(rrs_app PRIVATE ${CURSES_LIBRARIES} rrs_auth rrs_search rrs_inventory rrs_booking)

enable_testing()
find_package(GTest REQUIRED)

add_executable(rrs_booking_tests tests/test_booking.cpp)
target_include_directories(rrs_booking_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/booking"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/auth"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/inventory"
)
target_link_libraries(rrs_booking_tests PRIVATE rrs_booking rrs_inventory rrs_auth Threads::Threads GTest::gtest_main)
add_test(NAME rrs_booking_tests COMMAND rrs_booking_tests)

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

add_executable(rrs_fare_eco_tests tests/test_fare_eco.cpp)
target_link_libraries(rrs_fare_eco_tests PRIVATE rrs_search GTest::gtest_main)
add_test(NAME rrs_fare_eco_tests COMMAND rrs_fare_eco_tests)

# NFR-001 / FR-017 / NFR-008 performance check (ST-02)
add_executable(rrs_search_perf_tests tests/test_search_perf.cpp)
target_link_libraries(rrs_search_perf_tests PRIVATE rrs_search GTest::gtest_main)
add_test(NAME rrs_search_perf_tests COMMAND rrs_search_perf_tests)

# Security tests (SEC-003, SEC-004, SEC-006): every .cpp in tests/security/ is built in.
file(GLOB RRS_SECURITY_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/security/*.cpp")
add_executable(rrs_security_tests ${RRS_SECURITY_TESTS})
target_include_directories(rrs_security_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/auth"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/audit"
)
target_link_libraries(rrs_security_tests PRIVATE rrs_auth GTest::gtest_main)
add_test(NAME rrs_security_tests COMMAND rrs_security_tests)

# NFR-008: 500 trains and 10,000 bookings
add_executable(rrs_scalability_tests tests/test_scalability.cpp)
target_include_directories(rrs_scalability_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/auth"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/inventory"
)
target_link_libraries(rrs_scalability_tests PRIVATE rrs_booking rrs_inventory rrs_search rrs_auth GTest::gtest_main)
add_test(NAME rrs_scalability_tests COMMAND rrs_scalability_tests)

# Security validation pass (#36): SEC-001 to SEC-006 end to end
add_executable(rrs_security_validation_tests tests/test_security_validation.cpp)
target_include_directories(rrs_security_validation_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/auth"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/audit"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/inventory"
)
target_link_libraries(rrs_security_validation_tests PRIVATE rrs_booking rrs_inventory rrs_search rrs_auth GTest::gtest_main)
add_test(NAME rrs_security_validation_tests COMMAND rrs_security_validation_tests)

# NFR-005: a payment-timeout retry never creates a second booking (#40)
add_executable(rrs_payment_retry_tests tests/test_payment_retry.cpp)
target_include_directories(rrs_payment_retry_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/auth"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/booking"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/inventory"
)
target_link_libraries(rrs_payment_retry_tests PRIVATE rrs_booking rrs_inventory rrs_search rrs_auth GTest::gtest_main)
add_test(NAME rrs_payment_retry_tests COMMAND rrs_payment_retry_tests)
