cmake_minimum_required(VERSION 3.12)

# Change your executable name to something creative!
set(NAME snowman) # <-- Name your project/executable here!
set(PICO_BOARD pico2_w)

include("$ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake")

# Gooey boilerplate
project(${NAME} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(PICO_CXX_ENABLE_EXCEPTIONS 1)

# Initialize the SDK
pico_sdk_init()

SET(LIB_DIR "${CMAKE_CURRENT_LIST_DIR}/../lib")

#include Libraries
set(PICO_SERVO_PATH "${LIB_DIR}/pico-servo")
include(pico-servo.cmake)
set(EIGEN_PATH "${LIB_DIR}/eigen")
include(eigen.cmake)
include(${LIB_DIR}/PicoLED/PicoLed.cmake)
set(KW307_DIR "${LIB_DIR}/KW307_SDK v2.5.1 Apache 2.0/KW307_SDK_base")
include(kw307.cmake)

#Add main source directory
add_subdirectory(src)

#Set up files for the release packages
install(CODE "execute_process(COMMAND $ENV{HOME}/bin/picoDeploy.sh ${CMAKE_CURRENT_BINARY_DIR}/src/${NAME}.elf)")

# Set up files for the release packages
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/src/${NAME}.uf2
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
)

set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_GENERATOR "ZIP" "TGZ")
include(CPack)
