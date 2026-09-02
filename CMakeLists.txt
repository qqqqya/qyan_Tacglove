cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME M0_Init)

#
# Firmware artifact/version tag: this is the single source of truth used by
# both the MCU HELLO firmware tag and the generated ELF/HEX/BIN file names.
# Increment this tag only when a new firmware revision is intentionally issued.
#
set(FIRMWARE_BUILD_TAG "STAGE3_R1")

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# External FreeRTOS kernel supplied beside this CubeMX project.
set(FREERTOS_KERNEL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../FreeRTOS")

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    BSP/LED/bsp_led_driver.c
    BSP/LED/bsp_led_handler.c
    BSP/BEEP/bsp_beep_driver.c
    BSP/BEEP/bsp_beep_handler.c
    BSP/KEY/bsp_key_handler.c
    BSP/UART/bsp_uart_driver.c
    Middlewares/Protocol/mcu_protocol.c
    User_tasks/communication_task.c
    User_tasks/led_task.c
    User_tasks/key_task.c
    User_tasks/system_event.c
    User_tasks/system_status.c
    User_tasks/task_manager.c
    Middlewares/FreeRTOS/os_hooks.c

    ${FREERTOS_KERNEL_DIR}/list.c
    ${FREERTOS_KERNEL_DIR}/queue.c
    ${FREERTOS_KERNEL_DIR}/tasks.c
    ${FREERTOS_KERNEL_DIR}/portable/MemMang/heap_4.c
    ${FREERTOS_KERNEL_DIR}/portable/GCC/ARM_CM0/port.c
    ${FREERTOS_KERNEL_DIR}/portable/GCC/ARM_CM0/portasm.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    BSP/LED
    BSP/BEEP
    BSP/KEY
    BSP/UART
    Middlewares/Protocol
    User_tasks
    Middlewares/FreeRTOS
    ${FREERTOS_KERNEL_DIR}/include
    ${FREERTOS_KERNEL_DIR}/portable/GCC/ARM_CM0
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    FIRMWARE_BUILD_TAG="${FIRMWARE_BUILD_TAG}"
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)

# Generate reproducible, versioned programming artifacts after every build.
set(FIRMWARE_ARTIFACT_BASENAME
    "${CMAKE_PROJECT_NAME}_${FIRMWARE_BUILD_TAG}"
)

add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>"
            "${CMAKE_CURRENT_BINARY_DIR}/${FIRMWARE_ARTIFACT_BASENAME}.elf"
    COMMAND ${CMAKE_OBJCOPY} -O ihex
            "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>"
            "${CMAKE_CURRENT_BINARY_DIR}/${FIRMWARE_ARTIFACT_BASENAME}.hex"
    COMMAND ${CMAKE_OBJCOPY} -O binary
            "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>"
            "${CMAKE_CURRENT_BINARY_DIR}/${FIRMWARE_ARTIFACT_BASENAME}.bin"
    COMMENT "Generating ${FIRMWARE_ARTIFACT_BASENAME}.elf/.hex/.bin"
    VERBATIM
)

#
# Provide a safe one-command programming target only for Release builds.
# This prevents accidentally flashing build/cmake/Debug/M0_Init.elf when the
# board is expected to run the timing- and memory-validated Release firmware.
#
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    find_program(OPENOCD_EXECUTABLE openocd)
    if(OPENOCD_EXECUTABLE)
        add_custom_target(flash
            COMMAND ${OPENOCD_EXECUTABLE}
                    -f "${CMAKE_SOURCE_DIR}/openocd.cfg"
                    -c "gdb port disabled"
                    -c "init; reset init"
                    -c "program $<TARGET_FILE:${CMAKE_PROJECT_NAME}> verify reset exit"
            DEPENDS ${CMAKE_PROJECT_NAME}
            COMMENT "Flashing validated Release firmware: $<TARGET_FILE:${CMAKE_PROJECT_NAME}>"
            USES_TERMINAL
            VERBATIM
        )
    else()
        message(WARNING "OpenOCD not found: Release 'flash' target is unavailable")
    endif()
endif()
