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

# ===== Compatible Layer (USB CDC+MSC 듀얼 모드) =====
# STM32 CubeMX는 USB Device(CDC)와 Host(MSC)를 동시에 자동생성할 수 없습니다.
# Compatible/ 폴더에 USB 라이브러리와 설정 파일을 모아
# 하나의 USB OTG 포트에서 CDC/MSC 듀얼 모드를 구현합니다.
# USB HAL 드라이버(PCD/HCD/LL_USB)는 표준 Drivers/에서 컴파일됩니다.

# Compatible include paths (stm32cubemx INTERFACE에 추가 → 모든 타겟에서 접근 가능)
target_include_directories(stm32cubemx INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/STM32_USB_Device_Library/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/STM32_USB_Device_Library/Class/CDC/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/STM32_USB_Host_Library/Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/STM32_USB_Host_Library/Class/MSC/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/USB_DEVICE/App
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/USB_DEVICE/Target
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/USB_HOST/App
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/USB_HOST/Target
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/FATFS/Target
)

# Compatible layer helper (IRQ router + defines)
# USB HAL drivers (PCD/HCD/LL_USB) are now compiled from standard Drivers/ via stm32cubemx.
target_sources(STM32_Drivers PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/stm32h7xx_compatible.c
)

# Compatible USB sources (Device CDC + Host MSC + FATFS USB diskio)
set(COMPATIBLE_USB_SRC
    # USB Device Library (CDC)
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/STM32_USB_Device_Library/Core/Src/usbd_core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/STM32_USB_Device_Library/Core/Src/usbd_ctlreq.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/STM32_USB_Device_Library/Core/Src/usbd_ioreq.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/STM32_USB_Device_Library/Class/CDC/Src/usbd_cdc.c
    # USB Host Library (MSC)
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/STM32_USB_Host_Library/Core/Src/usbh_core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/STM32_USB_Host_Library/Core/Src/usbh_ctlreq.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/STM32_USB_Host_Library/Core/Src/usbh_ioreq.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/STM32_USB_Host_Library/Core/Src/usbh_pipes.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/STM32_USB_Host_Library/Class/MSC/Src/usbh_msc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/STM32_USB_Host_Library/Class/MSC/Src/usbh_msc_bot.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/STM32_USB_Host_Library/Class/MSC/Src/usbh_msc_scsi.c
    # USB Device App (CDC interface, descriptor, config)
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/USB_DEVICE/App/usb_device.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/USB_DEVICE/App/usbd_cdc_if.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/USB_DEVICE/App/usbd_desc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/USB_DEVICE/Target/usbd_conf.c
    # USB Host App (MSC host)
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/USB_HOST/App/usb_host.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/USB_HOST/Target/usbh_conf.c
    # FATFS USB Host diskio
    ${CMAKE_CURRENT_SOURCE_DIR}/Compatible/FATFS/Target/usbh_diskio.c
)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# ===== Data Map Code-Gen (YAML → C header) =====
# xm_total_data.yaml이 변경되면 xm_total_data_packet.h를 자동 재생성합니다.
# Python 3.x + pyyaml 필요 (pip install pyyaml)
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
    set(DATA_MAP_YAML  ${CMAKE_CURRENT_SOURCE_DIR}/tools/data_map/xm_total_data.yaml)
    set(DATA_MAP_GEN   ${CMAKE_CURRENT_SOURCE_DIR}/tools/data_map/generate_data_map.py)
    set(DATA_MAP_HDR   ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Comm/USB/xm_total_data_packet.h)

    add_custom_command(
        OUTPUT  ${DATA_MAP_HDR}
        COMMAND ${Python3_EXECUTABLE} ${DATA_MAP_GEN} ${DATA_MAP_YAML}
                --c-out ${DATA_MAP_HDR}
        DEPENDS ${DATA_MAP_YAML} ${DATA_MAP_GEN}
        COMMENT "[Data Map] Generating xm_total_data_packet.h from YAML SSOT"
        VERBATIM
    )
    add_custom_target(data_map_gen DEPENDS ${DATA_MAP_HDR})
else()
    message(WARNING "Python3 not found — Data Map Code-Gen disabled. "
                    "xm_total_data_packet.h will NOT be auto-regenerated.")
    add_custom_target(data_map_gen)  # empty target for dependency chain
endif()

# ===== User-defined source files =====

# AGR_MW - DOP (DOP V3 Core + Transport/CAN_FD + Transport/CoE + NMT)
# 모든 Transport 소스를 무조건 포함합니다.
# 비활성 Transport는 agr_dop_config.h 매크로에 의해 빈 번역 단위로 컴파일됩니다.
set(AGR_DOP_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/AGR_MW/DOP/agr_nmt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/AGR_MW/DOP/Core/agr_od.c
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/AGR_MW/DOP/Core/agr_sdo_protocol.c
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/AGR_MW/DOP/Core/agr_pdo_engine.c
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/AGR_MW/DOP/Transport/CAN_FD/agr_dop_canfd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/AGR_MW/DOP/Transport/CoE/agr_dop_coe_slave.c
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/AGR_MW/DOP/Transport/CoE/agr_dop_coe_master.c
)

# AGR_MW - PnP
set(AGR_PNP_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/AGR_MW/PnP/agr_pnp_master.c
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/AGR_MW/PnP/agr_pnp_slave.c
)

# AGR_MW - BOOT Core (common boot infrastructure)
set(AGR_BOOT_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/AGR_MW/BOOT/Src/agr_boot_core.c
)

# System/Boot - XM-specific boot trigger + fw_info
set(SYSTEM_BOOT_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Boot/boot_ftp_trigger.c
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Boot/boot_fw_info.c
)

# Services - DOP_Mngr (Data Object Manager)
set(DOP_MNGR_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/Services/DOP_Mngr/Src/data_object.c
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/Services/DOP_Mngr/Src/data_object_common.c
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/Services/DOP_Mngr/Src/data_object_dictionaries.c
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/Services/DOP_Mngr/Src/data_object_interface.c
)

# AGR_MW - RiskMngr (Error Dictionary)
set(RISK_MNGR_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/AGR_MW/RiskMngr/Src/risk_mngr.c
)

# AGR_MW - TSM (Task State Machine)
set(TSM_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/AGR_MW/TSM/Src/routine_mngr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/AGR_MW/TSM/Src/task_mngr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/AGR_MW/TSM/Src/task_state_machine.c
)

# XM API
set(XM_API_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/XM_API/xm_api.c
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/XM_API/xm_api_data.c
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/XM_API/xm_api_external_io.c
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/XM_API/xm_api_led_btn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/XM_API/xm_api_tsm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/XM_API/xm_api_usb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/XM_API/xm_api_freertos.c
)

# Devices - AGR (Angel Robotics)
set(DEVICES_AGR_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/Devices/AGR/Control_Module/cm_drv.c
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/Devices/AGR/IMU_Module/imu_hub_drv.c
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/Devices/AGR/EMG_Module/emg_hub_drv.c
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/Devices/AGR/FES_Module/fes_hub_drv.c
)

# Devices - Marveldex
set(DEVICES_MARVELDEX_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/Devices/Marveldex/MDAF-25-6850.c
)

# Devices - Xsens
set(DEVICES_XSENS_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/Devices/Xsens/mti-630.c
)

# System - Links
# [V2.0] Link Layer 완전 제거 - Device로 합병됨
# GRF → mdaf-25-6850.c (Auto-Sense + DataLake)
# XSENS → mti-630.c (Auto-Sense + DataLake)
# PnP Manager → pnp_task.c (System Core)
# CM Link → cm_drv.c (이미 합병 완료)
set(SYSTEM_LINKS_SRC
    # 모든 Link 파일 삭제됨 - Device/System Core로 이관
)

# System - Comm
set(SYSTEM_COMM_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Comm/CANFD/canfd_rx_handler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Comm/UART/uart_rx_handler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Comm/USB/cdc_handler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Comm/USB/data_logger.c
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Comm/USB/usb_mode_handler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Comm/USB/phai_packet_builder.c
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Comm/USB/xm_total_data.c
)

# System - Core
set(SYSTEM_CORE_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Core/system_startup.c
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Core/core_process.c
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Core/pnp_task.c
)

# System - Board
set(SYSTEM_BOARD_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Board/Button/button_manager.c
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Board/LED/led_manager.c
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Board/GPIO/external_io.c
)

# Application Layer
set(APP_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_Apps/User_Algorithm/user_app.c
)

# IOIF Layer (Git Submodule: AGR-EXO/IOIF)
# 각 .c 파일은 AGRB_IOIF_XXX_ENABLE 매크로로 조건부 컴파일됨
# 미활성 모듈은 빈 번역 단위로 컴파일 (오버헤드 없음)
file(GLOB IOIF_SRC "${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/IOIF/Src/*.c")

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # AGR_MW (common middleware)
    ${AGR_BOOT_SRC}
    ${AGR_DOP_SRC}
    ${AGR_PNP_SRC}

    # System/Boot (XM-specific boot trigger)
    ${SYSTEM_BOOT_SRC}
    ${DOP_MNGR_SRC}
    ${RISK_MNGR_SRC}
    ${TSM_SRC}
    ${XM_API_SRC}

    # Devices
    ${DEVICES_AGR_SRC}
    ${DEVICES_MARVELDEX_SRC}
    ${DEVICES_XSENS_SRC}

    # System - Links
    ${SYSTEM_LINKS_SRC}

    # System - Comm
    ${SYSTEM_COMM_SRC}

    # System - Core
    ${SYSTEM_CORE_SRC}

    # System - Board
    ${SYSTEM_BOARD_SRC}

    # Application Layer
    ${APP_SRC}

    # IOIF Layer
    ${IOIF_SRC}

    # Compatible (USB CDC+MSC 듀얼 모드)
    ${COMPATIBLE_USB_SRC}
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # User-defined include paths
    # AGR_MW (submodule)
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/AGR_MW/BOOT/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/AGR_MW/DOP
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/AGR_MW/DOP/Core
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/AGR_MW/DOP/Transport/CAN_FD
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/AGR_MW/DOP/Transport/CoE
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/AGR_MW/DOP/Transport/Serial
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/AGR_MW/DOP/Transport/UDP
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/AGR_MW/PnP
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/AGR_MW/RiskMngr/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/AGR_MW/TSM/Inc
    # System/Boot (XM-specific boot trigger)
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Boot
    # Services (module-specific)
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/Services/DOP_Mngr/Inc

    # Devices
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/Devices/AGR/Control_Module
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/Devices/AGR/IMU_Module
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/Devices/AGR/EMG_Module
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/Devices/AGR/FES_Module
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/Devices/Marveldex
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/Devices/XSENS

    # System
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Comm/CANFD
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Comm/UART
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Comm/USB
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Core
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Board
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Board/Button
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Board/LED
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Board/GPIO
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Config

    # XM API
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/XM_API

    # Application & IOIF
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_Apps/User_Algorithm
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/IOIF/Inc
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # IOIF Submodule: 사용할 모듈만 활성화 (미활성 모듈은 빈 번역 단위)
    AGRB_IOIF_FDCAN_ENABLE
    AGRB_IOIF_UART_ENABLE
    AGRB_IOIF_GPIO_ENABLE
    AGRB_IOIF_TIM_ENABLE
    AGRB_IOIF_DWT_ENABLE
    AGRB_IOIF_USB_ENABLE
    AGRB_IOIF_ADC_ENABLE
    AGRB_IOIF_FILESYSTEM_ENABLE
    # AGRB_IOIF_SPI_ENABLE       # 필요시 활성화
    # AGRB_IOIF_I2C_ENABLE       # 필요시 활성화
    # AGRB_IOIF_DMA_ENABLE       # SPI/I2C 사용 시 필요
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Data Map Code-Gen dependency — YAML 변경 시 .h 자동 재생성 후 빌드
add_dependencies(${CMAKE_PROJECT_NAME} data_map_gen)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)