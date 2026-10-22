add_executable(${NAME}
        main.cpp   
        Arm.cpp
        Kinematics.cpp
        Anim.cpp
        ArmWave.cpp
        ArmHug.cpp
        Head.cpp
        HeadNod.cpp
        HeadShake.cpp
        HeadLook.cpp
        HeadSleep.cpp
        Dance.cpp
        WakeUp.cpp
        Star.cpp
        StarChase.cpp
        StarHeartbeat.cpp
        )

# Pull in our pico_stdlib which pulls in commonly used features
target_link_libraries(${NAME} 
	 pico_stdlib
     picoServo
     eigen
     PicoLed
	)
	
     

# create map/bin/hex file etc.
pico_add_extra_outputs(${NAME})

# enable usb output, disable uart output
pico_enable_stdio_usb(${NAME} 1)
pico_enable_stdio_uart(${NAME} 1)
target_compile_definitions(${NAME} PRIVATE
    PICO_DEFAULT_UART_RX_PIN=17
    PICO_DEFAULT_UART_TX_PIN=16
)