# Add library cpp files

if (NOT DEFINED PICO_SERVO_PATH)
    set(PICO_SERVO_PATH "${CMAKE_CURRENT_LIST_DIR}/lib/pico-servo")
endif()
if (NOT DEFINED PICO_SERVO_PORT)
	set(COREHTTP_PORT "${CMAKE_CURRENT_LIST_DIR}/port/pico-servo")
endif()


add_library(picoServo STATIC)

target_sources(picoServo PUBLIC
	${PICO_SERVO_PATH}/src/servo.c
)


# Add include directory
target_include_directories(picoServo PUBLIC 
    ${PICO_SERVO_PATH}/src
    ${PICO_SERVO_PORT}
)

target_link_libraries(picoServo 
	PUBLIC 
	pico_stdlib
	hardware_pwm
    hardware_irq
	)


