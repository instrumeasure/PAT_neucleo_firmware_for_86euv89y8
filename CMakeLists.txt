# PAT Nucleo H753 — canonical build: CMake + STM32Cube H7 HAL pack (see AGENTS.md).
# Requires: CMake 3.22+, arm-none-eabi-gcc, STM32CubeH7 firmware tree.
#
# Configure (PowerShell):
#   cmake -B cmake-build -G Ninja `
#     -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake `
#     -DSTM32_CUBE_H7_FW="$env:USERPROFILE\.platformio\packages\framework-stm32cubeh7"
#
# Build:
#   cmake --build cmake-build
#
# Outputs: cmake-build/pat_nucleo_h753.elf, .bin
#
cmake_minimum_required(VERSION 3.22)

if(NOT CMAKE_TOOLCHAIN_FILE)
  message(FATAL_ERROR "Pass -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake")
endif()

project(pat_nucleo_h753 C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# STM32CubeH7 pack: default to PlatformIO cache if unset (after one `pio run`).
if(NOT STM32_CUBE_H7_FW)
  if(DEFINED ENV{STM32_CUBE_H7_FW})
    set(STM32_CUBE_H7_FW "$ENV{STM32_CUBE_H7_FW}")
  elseif(DEFINED ENV{USERPROFILE})
    file(TO_CMAKE_PATH "$ENV{USERPROFILE}/.platformio/packages/framework-stm32cubeh7" STM32_CUBE_H7_FW)
  endif()
endif()
file(TO_CMAKE_PATH "${STM32_CUBE_H7_FW}" STM32_CUBE_H7_FW)

if(NOT EXISTS "${STM32_CUBE_H7_FW}/Drivers/CMSIS/Include/cmsis_compiler.h")
  message(FATAL_ERROR
    "STM32_CUBE_H7_FW must point to an STM32CubeH7 firmware pack (Drivers/, Middlewares/, ...).\n"
    "  Example: -DSTM32_CUBE_H7_FW=\"$ENV{USERPROFILE}/.platformio/packages/framework-stm32cubeh7\"\n"
    "  Or install STM32Cube FW_H7 from ST and pass that path.")
endif()

set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/cube/linker/STM32H753ZITx_FLASH.ld")
if(NOT EXISTS "${LINKER_SCRIPT}")
  message(FATAL_ERROR "Missing linker script: ${LINKER_SCRIPT}")
endif()

set(CPU_FLAGS
  -mcpu=cortex-m7
  -mthumb
  -mfpu=fpv5-d16
  -mfloat-abi=hard
)

add_compile_options(
  ${CPU_FLAGS}
  -Os
  -ffunction-sections
  -fdata-sections
  -Wall
)

add_compile_definitions(
  STM32H7
  STM32H7xx
  STM32H753xx
  USE_HAL_DRIVER
)

include_directories(
  "${STM32_CUBE_H7_FW}/Drivers/CMSIS/Include"
  "${STM32_CUBE_H7_FW}/Drivers/CMSIS/Device/ST/STM32H7xx/Include"
  "${STM32_CUBE_H7_FW}/Drivers/STM32H7xx_HAL_Driver/Inc"
  "${STM32_CUBE_H7_FW}/Drivers/STM32H7xx_HAL_Driver/Inc/Legacy"
  "${CMAKE_SOURCE_DIR}/include"
  "${CMAKE_SOURCE_DIR}/lib/ADS127TI/include"
)

file(GLOB HAL_SOURCES CONFIGURE_DEPENDS
  "${STM32_CUBE_H7_FW}/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal*.c"
)
list(FILTER HAL_SOURCES EXCLUDE REGEX ".*[Tt]emplate.*")

set(STARTUP_ASM "${STM32_CUBE_H7_FW}/Drivers/CMSIS/Device/ST/STM32H7xx/Source/Templates/gcc/startup_stm32h753xx.s")
set(SYSTEM_SRC "${STM32_CUBE_H7_FW}/Drivers/CMSIS/Device/ST/STM32H7xx/Source/Templates/system_stm32h7xx.c")

set(APP_SOURCES
  "${CMAKE_SOURCE_DIR}/src/main.c"
  "${CMAKE_SOURCE_DIR}/src/stm32h7xx_it.c"
  "${CMAKE_SOURCE_DIR}/src/ads127l11_hal_stm32.c"
  "${CMAKE_SOURCE_DIR}/src/app_state.c"
  "${CMAKE_SOURCE_DIR}/src/qpd_dsp.c"
  "${CMAKE_SOURCE_DIR}/src/qpd_spi6_slave.c"
  "${CMAKE_SOURCE_DIR}/lib/ADS127TI/src/ads127l11.c"
)

add_executable(pat_nucleo_h753
  ${STARTUP_ASM}
  ${SYSTEM_SRC}
  ${HAL_SOURCES}
  ${APP_SOURCES}
)

set_target_properties(pat_nucleo_h753 PROPERTIES SUFFIX ".elf")

set_source_files_properties(${STARTUP_ASM} PROPERTIES LANGUAGE ASM)

target_compile_options(pat_nucleo_h753 PRIVATE $<$<COMPILE_LANGUAGE:ASM>:${CPU_FLAGS}>)

target_link_options(pat_nucleo_h753 PRIVATE
  ${CPU_FLAGS}
  -T${LINKER_SCRIPT}
  -Wl,--gc-sections
  -Wl,-Map=${CMAKE_BINARY_DIR}/pat_nucleo_h753.map
  --specs=nano.specs
  --specs=nosys.specs
)

add_custom_command(TARGET pat_nucleo_h753 POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:pat_nucleo_h753> ${CMAKE_BINARY_DIR}/pat_nucleo_h753.bin
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:pat_nucleo_h753>
  COMMENT "Generating .bin and size"
)
