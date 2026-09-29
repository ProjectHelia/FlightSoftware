include(FetchContent)
FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

# One executable per library; add a line here per new test file
function(helia_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE helia_libs GTest::gtest_main)
    gtest_discover_tests(${name})
endfunction()

helia_test(test_fsm libs/test_fsm.cpp)
helia_test(test_can libs/test_can.cpp)
helia_test(test_utilities libs/test_utilities.cpp)
helia_test(test_util_thermistor libs/test_util_thermistor.cpp)
helia_test(test_can_eps libs/test_can_eps.cpp)

helia_test(test_eps src/test_eps.cpp)
target_link_libraries(test_eps PRIVATE node_EPS)

helia_test(test_master src/test_master.cpp)
target_link_libraries(test_master PRIVATE node_Master)

helia_test(test_instrumentation src/test_instrumentation.cpp)
target_link_libraries(test_instrumentation PRIVATE node_Instrumentation)