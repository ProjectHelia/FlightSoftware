# ESP-IDF reads this file twice. The first "early expansion" pass only wants
# to know which IDF components we depend on, and can't run file(GLOB ...).
if(CMAKE_BUILD_EARLY_EXPANSION)
    idf_component_register(REQUIRES driver esp_adc)
    return()
endif()

if(NOT DEFINED HELIA_NODE)
    message(FATAL_ERROR "HELIA_NODE not set")
endif()

include(${CMAKE_CURRENT_LIST_DIR}/../libs/libs.cmake)

set(NODE_DIR ${CMAKE_CURRENT_LIST_DIR}/${HELIA_NODE})
file(GLOB NODE_SRCS ${NODE_DIR}/*.c ${NODE_DIR}/HAL/esp32/*.c)

idf_component_register(
    SRCS         ${NODE_SRCS} ${HELIA_LIB_SRCS} ${HELIA_LIB_ESP32_SRCS}
    INCLUDE_DIRS ${NODE_DIR} ${NODE_DIR}/HAL ${NODE_DIR}/HAL/esp32 ${HELIA_LIB_INCS}
    REQUIRES     driver esp_adc esp_eth esp_netif esp_event lwip)