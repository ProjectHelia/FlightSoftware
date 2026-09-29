set(HELIA_LIB_DIR ${CMAKE_CURRENT_LIST_DIR})

# Pure helpers: built everywhere
file(GLOB HELIA_LIB_SRCS CONFIGURE_DEPENDS ${HELIA_LIB_DIR}/*/*.c)

# Platform-specific helpers: libs/<lib>/esp32/*.c or libs/<lib>/sim/*.c
file(GLOB HELIA_LIB_ESP32_SRCS CONFIGURE_DEPENDS ${HELIA_LIB_DIR}/*/esp32/*.c)
file(GLOB HELIA_LIB_SIM_SRCS   CONFIGURE_DEPENDS ${HELIA_LIB_DIR}/*/sim/*.c)

set(HELIA_LIB_INCS
    ${HELIA_LIB_DIR}/fsm
    ${HELIA_LIB_DIR}/can
    ${HELIA_LIB_DIR}/utilities) # Add more here as they're implemented :> 