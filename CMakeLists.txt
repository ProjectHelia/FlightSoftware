cmake_minimum_required(VERSION 3.16)

if(NOT HELIA_HOST)
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    project(FlightSoftware)
    return()
endif()

project(FlightSoftware C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# This still needs to be tested
option(HELIA_SANITIZE "Build with ASAN + UBSan" ON)
if(HELIA_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

add_library(helia_warnings INTERFACE)

# Don't add -Werror as it will cause too many headaches ;-;
target_compile_options(helia_warnings INTERFACE -Wall -Wextra -Wpedantic -Wconversion)

include(libs/libs.cmake)
add_library(helia_libs STATIC ${HELIA_LIB_SRCS})
target_include_directories(helia_libs PUBLIC ${HELIA_LIB_INCS})
target_link_libraries(helia_libs PRIVATE helia_warnings)
if(NOT APPLE)
    target_link_libraries(helia_libs PUBLIC m)
endif()

# Each node's logic + sim HAL as a library for tests (setup.c is the task
# loop / entry point, so it's left out until the simulator step)
function(helia_node name)
    set(dir ${CMAKE_SOURCE_DIR}/src/${name})
    file(GLOB srcs CONFIGURE_DEPENDS ${dir}/*.c ${dir}/HAL/sim/*.c)
    list(FILTER srcs EXCLUDE REGEX "/setup\\.c$")
    add_library(node_${name} STATIC ${srcs})
    target_include_directories(node_${name} PUBLIC ${dir} ${dir}/HAL)
    target_link_libraries(node_${name} PUBLIC helia_libs PRIVATE helia_warnings)
endfunction()



# TODO(remy) add the rest of the nodes here, one by one as they're implemented
helia_node(EPS)
helia_node(Master)
helia_node(Instrumentation)

enable_testing()
add_subdirectory(tests)
