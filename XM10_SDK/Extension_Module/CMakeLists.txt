cmake_minimum_required(VERSION 3.22)

# ============================================================================
# Extension_Module SDK (v2.0.0)
#
# XM_FW 펌웨어 코드는 libXM_Lib.a 정적 라이브러리로 제공됩니다.
# 사용자는 XM_Apps/User_Algorithm/user_app.c 만 수정하여 빌드합니다.
# ============================================================================

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME Extension_Module)

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

# ===== XM_FW Static Library (libXM_Lib.a) =====
# XM_FW 소스 + Compatible(USB) + IOIF 모두 libXM_Lib.a에 포함
add_library(XM_Lib STATIC IMPORTED)
set_target_properties(XM_Lib PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/libXM_Lib.a
)

# ===== User Application Source =====
set(APP_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_Apps/User_Algorithm/user_app.c
)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${APP_SRC}
)

# ===== Include Paths (XM_FW headers) =====
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # XM_API (user-facing API)
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/XM_API

    # Services
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/Services/AGR_DOP
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/Services/AGR_PnP
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/Services/Buff_Mngr/Ring_Buffer/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/Services/DOP_Mngr/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/Services/Risk_Mngr/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/Services/Task_State_Machine/Inc

    # Devices
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/Devices/AGR/Control_Module
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/Devices/AGR/IMU_Module
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/Devices/Marveldex
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/Devices/XSENS

    # System
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Board/Button
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Board/GPIO
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Board/LED
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Comm/CANFD
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Comm/UART
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Comm/USB
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Config
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Core

    # IOIF
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/IOIF/Inc

    # Application
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_Apps/User_Algorithm

    # Compatible (USB Device/Host libraries)
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/Drivers/STM32H7xx_HAL_Driver/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/FATFS/Target
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/STM32_USB_Device_Library/Class/CDC/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/STM32_USB_Device_Library/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/STM32_USB_Host_Library/Class/MSC/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/STM32_USB_Host_Library/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/USB_DEVICE/App
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/USB_DEVICE/Target
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/USB_HOST/App
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/USB_HOST/Target
)

# ===== Compile Definitions =====
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # RTOS
    USE_OS_RTOS

    # Memory Configuration
    DATA_IN_D2_SRAM

    # IOIF Submodule V3.0: ioif_conf.h에서 관리
    # (별도 -D 플래그 불필요 — ioif_conf.h 참조)
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# ===== Link Libraries =====
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # XM_FW Static Library
    XM_Lib
)
