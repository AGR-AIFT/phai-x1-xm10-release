# ==============================================================================
# STM32 ARM GCC Toolchain File for CMake
# ==============================================================================
# Usage: cmake -B build -DCMAKE_TOOLCHAIN_FILE=tools/stm32_gcc_toolchain.cmake
#
# This file is shared across all Angel Robotics STM32 modules (XM, CM, MD, SM).
# MCU-specific flags are set in the generated CMakeLists.txt, not here.
# ==============================================================================

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# --- Toolchain prefix ---
set(TOOLCHAIN_PREFIX "arm-none-eabi-")

# Try to find the compiler in PATH first, then fall back to STM32CubeIDE bundled toolchain
find_program(CMAKE_C_COMPILER "${TOOLCHAIN_PREFIX}gcc")
if(NOT CMAKE_C_COMPILER)
    # STM32CubeIDE bundled ARM GCC (Windows default path)
    file(GLOB _CUBEIDE_GCC_DIRS
        "C:/ST/STM32CubeIDE*/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.*.win32_*/tools/bin"
    )
    list(SORT _CUBEIDE_GCC_DIRS ORDER DESCENDING)
    foreach(_dir ${_CUBEIDE_GCC_DIRS})
        if(EXISTS "${_dir}/${TOOLCHAIN_PREFIX}gcc.exe")
            set(TOOLCHAIN_PATH "${_dir}")
            break()
        endif()
    endforeach()
    if(TOOLCHAIN_PATH)
        set(CMAKE_C_COMPILER "${TOOLCHAIN_PATH}/${TOOLCHAIN_PREFIX}gcc.exe")
    endif()
endif()

set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})
find_program(CMAKE_CXX_COMPILER "${TOOLCHAIN_PREFIX}g++")
find_program(CMAKE_OBJCOPY "${TOOLCHAIN_PREFIX}objcopy")
find_program(CMAKE_OBJDUMP "${TOOLCHAIN_PREFIX}objdump")
find_program(CMAKE_SIZE "${TOOLCHAIN_PREFIX}size")

# --- Disable compiler checks (cross-compilation) ---
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# --- Common flags for all STM32 targets ---
set(CMAKE_C_FLAGS_INIT "-ffunction-sections -fdata-sections -Wall")
set(CMAKE_C_FLAGS_DEBUG_INIT "-g3 -O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE_INIT "-Os -DNDEBUG")
set(CMAKE_C_FLAGS_MINSIZEREL_INIT "-Os -DNDEBUG")
set(CMAKE_C_FLAGS_RELWITHDEBINFO_INIT "-Os -g -DNDEBUG")

set(CMAKE_ASM_FLAGS_INIT "-x assembler-with-cpp")

set(CMAKE_EXE_LINKER_FLAGS_INIT
    "-Wl,--gc-sections -static --specs=nosys.specs --specs=nano.specs -Wl,--allow-multiple-definition -Wl,--start-group -lc -lm -Wl,--end-group"
)
# Note: --allow-multiple-definition is needed for GCC 13.x where nano.specs
# provides _sbrk_r / __malloc_lock / __malloc_unlock that conflict with
# heap_useNewlib_ST.c (CubeIDE custom heap wrapper). The custom version
# takes precedence with this flag.
