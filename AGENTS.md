# AGENTS.md

## Project Context

This repository contains the XM10 Extension Module firmware for an STM32H743 target.
The project is C/ASM based, uses STM32CubeMX/HAL generated code, FreeRTOS, CAN-FD,
USB CDC/MSC support, FATFS, and project-specific XM firmware layers.

Primary build target name: `Extension_Module`.

## Important Directories

- `Core/`: STM32CubeMX generated application skeleton, startup integration, and user code sections.
- `Drivers/`: STM32 HAL/CMSIS driver sources.
- `Middlewares/`: third-party middleware such as FreeRTOS and USB/FATFS dependencies.
- `cmake/`: CMake toolchain and STM32CubeMX generated CMake integration.
- `Compatible/`: compatibility layer for USB CDC + MSC support and related FATFS/USB glue.
- `XM_FW/`: project firmware implementation, including system services, devices, communication, API, and middleware.
- `XM_Apps/`: application/user algorithm layer.
- `XM10_SDK/`: SDK-related project contents.
- `PythonDecoder/`: host-side Python tools for decoding, logging, or analysis.
- `tools/data_map/`: YAML-based data map generator. The generated C header is used by USB data packet code.
- `examples/`: example code and usage references.

## Build

Preferred CMake presets:

```powershell
cmake --preset Debug
cmake --build --preset Debug
```

Release build:

```powershell
cmake --preset Release
cmake --build --preset Release
```

The configured generator is Ninja and the ARM GCC toolchain file is:

```text
cmake/gcc-arm-none-eabi.cmake
```

Python 3 is used when available to regenerate:

```text
XM_FW/System/Comm/USB/xm_total_data_packet.h
```

from:

```text
tools/data_map/xm_total_data.yaml
```

The generator requires PyYAML when the data map is changed.

## Editing Rules

- Prefer the existing firmware architecture and naming style over introducing new abstractions.
- Keep changes tightly scoped to the requested behavior.
- Be careful with STM32CubeMX generated files. Preserve `USER CODE BEGIN` / `USER CODE END` sections and avoid editing generated regions unless explicitly required.
- Do not remove or simplify hardware initialization, interrupt handlers, DMA setup, linker scripts, or RTOS configuration without confirming the runtime impact.
- Treat `XM_FW/XM_API` as the public facade used by user applications when possible.
- Treat `XM_Apps/User_Algorithm` as the user-facing algorithm area when making application-level behavior changes.
- For data map changes, edit the YAML source of truth first, then regenerate or validate the generated header.
- Avoid broad formatting-only changes in generated or vendor files.
- Do not revert unrelated user changes in the worktree.

## Validation

When practical, run at least:

```powershell
cmake --build --preset Debug
```

If build prerequisites are missing locally, report the exact missing tool or failing command.

For changes involving generated data-map packets, also validate the generator path:

```powershell
python tools/data_map/generate_data_map.py tools/data_map/xm_total_data.yaml --c-out XM_FW/System/Comm/USB/xm_total_data_packet.h
```

Only run firmware flashing or debug-launch steps when the user explicitly asks for hardware interaction.

## Communication Notes

- The repository contains some Korean documentation/comments. Preserve existing wording and encoding when editing.
- Some files may show mojibake if opened with the wrong encoding. Do not rewrite large text blocks just to fix display issues unless the task is specifically about documentation encoding.
- Mention hardware assumptions explicitly when a change depends on connected STM32 hardware, KIT H10 firmware, CAN-FD devices, USB mode, sensors, or ST-Link availability.
