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

# ===== XM_FW Static Library (SDK Release) =====
# XM_FW 소스 코드는 libXM_Lib.a 정적 라이브러리로 사전 컴파일되어 배포됩니다.
# SDK 사용자는 헤더(.h)만 참조하고, 링크 시 .a 파일을 사용합니다.
set(XM_LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/libXM_Lib.a)

# Application Layer (SDK 사용자 코드)
set(APP_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_Apps/User_Algorithm/user_app.c
)

# boot_fw_info.c: .fw_header 섹션에 AGR_FwInfo_t를 배치하는 유일한 소스.
# libXM_Lib.a에 포함하면 링커 GC가 제거하므로 반드시 직접 컴파일해야 합니다.
# version.h에 의존하므로 SDK 사용자 버전이 자동 반영됩니다.
set(BOOT_FW_INFO_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/XM_FW/System/Boot/boot_fw_info.c
)

# Add sources to executable (XM_FW 소스 제거 — libXM_Lib.a로 대체)
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Boot FW Info (링커 .fw_header 섹션 — 직접 컴파일 필수)
    ${BOOT_FW_INFO_SRC}

    # Application Layer
    ${APP_SRC}

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
# NOTE: 아래 IOIF 매크로는 user_app.c 등 SDK 사용자 코드의 조건부 컴파일에만 영향을 줍니다.
# libXM_Lib.a 내부의 IOIF 드라이버는 라이브러리 빌드 시점에 이미 활성화되어 있으므로,
# 여기서 매크로를 변경해도 IOIF 드라이버 동작에는 영향이 없습니다.
# IOIF 모듈 활성화/비활성화는 XM_FW/System/Config/ioif_conf.h에서 관리됩니다.
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # IOIF Submodule: user_app.c에서 IOIF 헤더 조건부 include에 사용
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
# --whole-archive: HAL __weak 콜백을 libXM_Lib.a의 강한 심볼로 오버라이드 필수
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    -Wl,--whole-archive ${XM_LIB_PATH} -Wl,--no-whole-archive
)