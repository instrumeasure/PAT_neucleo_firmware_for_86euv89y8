cmake_minimum_required(VERSION 3.20)
file(READ "${CMAKE_SOURCE_DIR}/VERSION" _pat_fw_version_raw)
string(STRIP "${_pat_fw_version_raw}" PAT_FW_VERSION)
project(pat_nucleo_h753 VERSION "${PAT_FW_VERSION}" LANGUAGES C ASM)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# OFF (default): UART WARNING then continue if bring-up / post-START gate disagree (bench-friendly).
# ON: halt CPU when gate fails (re-run cmake with -DPAT_ADS127_STRICT_BRINGUP=ON).
option(PAT_ADS127_STRICT_BRINGUP "Halt on ADS127 bring-up or post-START shadow gate failure" OFF)

# ON: restore HAL_SPI_TransmitReceive for ADS127 RREG/WREG and 3-byte sample (A/B vs pat_spi_h7_master).
option(PAT_ADS127_SPI_HAL_LEGACY "ADS127 SPI: legacy HAL transfers (bisect)" OFF)

# SPI2 MISO (PC2 / PC2_C): default firmware clears PMCR.PC2SO (switch closed). If SPI2 bring-up
# fails and MISO stays high (DRDY timeouts, shadow all zero), reconfigure with this ON and rebuild.
option(PAT_SPI2_PC2SO_OPEN_INSTEAD "SPI2: SET PMCR.PC2SO (open); default is CLEAR (closed)" OFF)

# `pat_nucleo_quartet` always builds with **`PAT_QUARTET_PARALLEL_DRDY_WAIT=1`** (shared !CS + DRDY gate + parallel sample).
# DRDY gate is always SPI4 **PE15** (duplicate SDO/!DRDY net); no CMake toggle.

# TI SBAS946 §8.5.9: hold all J1 !CS low from init through nRESET so each ADS127 enters 3-wire SPI (no !CS high edges).
option(PAT_ADS127_SPI_3WIRE_CS_HELD_LOW "pat_nucleo_quartet: PAT_ADS127_SPI_3WIRE_CS_HELD_LOW=1 (TI 3-wire)" OFF)

# Throughput / LA correlate: UART `epoch_hz_est` from `span_us`; optional every-epoch print (UART flood).
option(PAT_QUARTET_DIAG_EPOCH_EVERY "pat_nucleo_quartet: PAT_QUARTET_DIAG_EPOCH_EVERY=1" OFF)

# Diag: per-bus `spi_master_rx3_zero_tx_unlocked` after shared DRDY (vs interleaved register or HAL SPI IT).
option(PAT_QUARTET_PARALLEL_SPI_RX3_SEQ_UNLOCKED "pat_nucleo_quartet: PAT_QUARTET_PARALLEL_SPI_RX3_SEQ_UNLOCKED=1" OFF)

# ON (default) = interleaved register quartet SPI (`pat_spi_h7_quartet_parallel_txrx_zero3_from_hspi`, no HAL SPI IT).
# OFF = HAL_SPI_TransmitReceive_IT + NVIC SPI1..4 IRQ.
option(PAT_QUARTET_PARALLEL_SPI_REGISTER_MASTER "pat_nucleo_quartet: register SPI sample in parallel epoch (no HAL SPI IT)" ON)

# Optional J2 export in quartet image: links SPI6 slave staging path and exposes rolling 64B frame (fmt 0x0A).
option(PAT_QUARTET_LINK_SPI6 "pat_nucleo_quartet: link SPI6 slave + qpd_spi6 staging for rolling payload" OFF)

set(PAT_SPI123_PRESCALER_DIV "64" CACHE STRING "pat_nucleo_quartet: SPI1–3 HAL baud prescaler divisor (8|16|32|64)")
set_property(CACHE PAT_SPI123_PRESCALER_DIV PROPERTY STRINGS 8 16 32 64)
if(NOT PAT_SPI123_PRESCALER_DIV MATCHES "^(8|16|32|64)$")
  message(FATAL_ERROR "PAT_SPI123_PRESCALER_DIV must be 8, 16, 32, or 64 (got '${PAT_SPI123_PRESCALER_DIV}').")
endif()

set(PAT_QUARTET_SPI_IRQ_PREEMPT_PRIORITY "6" CACHE STRING "pat_nucleo_quartet: SPI1–4 NVIC preemption priority 0–15 (lower = more urgent)")
if(NOT PAT_QUARTET_SPI_IRQ_PREEMPT_PRIORITY MATCHES "^([0-9]|1[0-5])$")
  message(FATAL_ERROR "PAT_QUARTET_SPI_IRQ_PREEMPT_PRIORITY must be 0..15 (got '${PAT_QUARTET_SPI_IRQ_PREEMPT_PRIORITY}').")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT DEFINED STM32_CUBE_H7_FW)
  set(STM32_CUBE_H7_FW "$ENV{STM32_CUBE_H7_FW}")
endif()
if(NOT STM32_CUBE_H7_FW OR NOT EXISTS "${STM32_CUBE_H7_FW}/Drivers/CMSIS/Include/cmsis_compiler.h")
  message(FATAL_ERROR "Set STM32_CUBE_H7_FW to STM32CubeH7 framework root (contains Drivers/CMSIS/...).")
endif()

set(CUBE "${STM32_CUBE_H7_FW}")
set(DEVICE_DIR "${CUBE}/Drivers/CMSIS/Device/ST/STM32H7xx")
set(HAL_DIR "${CUBE}/Drivers/STM32H7xx_HAL_Driver")

set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/cube/linker/STM32H753ZITx_FLASH.ld")

# Match CMSIS startup (.fpu softvfp); avoids FPU mode mismatch with startup_stm32h753xx.s
set(CPU_FLAGS
  -mcpu=cortex-m7
  -mthumb
  -mfloat-abi=soft
)

add_compile_options(
  ${CPU_FLAGS}
  -fdata-sections
  -ffunction-sections
  -Wall
  -Wextra
  -Wno-unused-parameter
  "$<$<CONFIG:Debug>:-Og>"
  "$<$<CONFIG:Debug>:-g3>"
  "$<$<CONFIG:Release>:-Os>"
  "$<$<CONFIG:Release>:-g0>"
)

add_compile_definitions(
  STM32H753xx
  USE_HAL_DRIVER
  HSE_VALUE=25000000
)

include_directories(
  "${CMAKE_SOURCE_DIR}/include"
  "${CMAKE_SOURCE_DIR}/src"
  "${CUBE}/Drivers/CMSIS/Include"
  "${DEVICE_DIR}/Include"
  "${HAL_DIR}/Inc"
  "${HAL_DIR}/Inc/Legacy"
)

set(HAL_SRC
  "${HAL_DIR}/Src/stm32h7xx_hal.c"
  "${HAL_DIR}/Src/stm32h7xx_hal_cortex.c"
  "${HAL_DIR}/Src/stm32h7xx_hal_rcc.c"
  "${HAL_DIR}/Src/stm32h7xx_hal_rcc_ex.c"
  "${HAL_DIR}/Src/stm32h7xx_hal_gpio.c"
  "${HAL_DIR}/Src/stm32h7xx_hal_pwr.c"
  "${HAL_DIR}/Src/stm32h7xx_hal_pwr_ex.c"
  "${HAL_DIR}/Src/stm32h7xx_hal_flash.c"
  "${HAL_DIR}/Src/stm32h7xx_hal_flash_ex.c"
  "${HAL_DIR}/Src/stm32h7xx_hal_spi.c"
  "${HAL_DIR}/Src/stm32h7xx_hal_spi_ex.c"
  "${HAL_DIR}/Src/stm32h7xx_hal_uart.c"
  "${HAL_DIR}/Src/stm32h7xx_hal_uart_ex.c"
  "${HAL_DIR}/Src/stm32h7xx_hal_tim.c"
  "${HAL_DIR}/Src/stm32h7xx_hal_tim_ex.c"
  "${HAL_DIR}/Src/stm32h7xx_hal_dma.c"
  "${HAL_DIR}/Src/stm32h7xx_hal_dma_ex.c"
  "${HAL_DIR}/Src/stm32h7xx_hal_exti.c"
)

set(CMSIS_SRC
  "${DEVICE_DIR}/Source/Templates/system_stm32h7xx.c"
  "${DEVICE_DIR}/Source/Templates/gcc/startup_stm32h753xx.s"
)

set(APP_SRC
  "${CMAKE_SOURCE_DIR}/src/main.c"
  "${CMAKE_SOURCE_DIR}/src/pat_clock.c"
  "${CMAKE_SOURCE_DIR}/src/pat_spi_ads127.c"
  "${CMAKE_SOURCE_DIR}/src/pat_spi_h7_master.c"
  "${CMAKE_SOURCE_DIR}/src/stm32h7xx_hal_msp.c"
  "${CMAKE_SOURCE_DIR}/src/stm32h7xx_it.c"
  "${CMAKE_SOURCE_DIR}/src/ads127l11.c"
)

add_executable(pat_nucleo_h753
  ${CMSIS_SRC}
  ${HAL_SRC}
  ${APP_SRC}
)

target_link_options(pat_nucleo_h753 PRIVATE
  ${CPU_FLAGS}
  -T${LINKER_SCRIPT}
  -Wl,--gc-sections
  -Wl,-Map=$<TARGET_FILE_DIR:pat_nucleo_h753>/pat_nucleo_h753.map
  --specs=nano.specs
  --specs=nosys.specs
  -Wl,--print-memory-usage
)

add_custom_command(TARGET pat_nucleo_h753 POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:pat_nucleo_h753>
          $<TARGET_FILE_DIR:pat_nucleo_h753>/pat_nucleo_h753.bin
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:pat_nucleo_h753>
)

# SPI6 J2 slave smoke test (separate ELF; flash with Flash-Stm32CubeOpenOCD.ps1 -Elf .../pat_nucleo_spi6.elf)
set(APP_SPI6_SRC
  "${CMAKE_SOURCE_DIR}/src/main_spi6.c"
  "${CMAKE_SOURCE_DIR}/src/spi6_test_frame.c"
  "${CMAKE_SOURCE_DIR}/src/pat_clock.c"
  "${CMAKE_SOURCE_DIR}/src/stm32h7xx_hal_msp.c"
  "${CMAKE_SOURCE_DIR}/src/stm32h7xx_it.c"
)

add_executable(pat_nucleo_spi6
  ${CMSIS_SRC}
  ${HAL_SRC}
  ${APP_SPI6_SRC}
)

target_link_options(pat_nucleo_spi6 PRIVATE
  ${CPU_FLAGS}
  -T${LINKER_SCRIPT}
  -Wl,--gc-sections
  -Wl,-Map=$<TARGET_FILE_DIR:pat_nucleo_spi6>/pat_nucleo_spi6.map
  --specs=nano.specs
  --specs=nosys.specs
  -Wl,--print-memory-usage
)

add_custom_command(TARGET pat_nucleo_spi6 POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:pat_nucleo_spi6>
          $<TARGET_FILE_DIR:pat_nucleo_spi6>/pat_nucleo_spi6.bin
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:pat_nucleo_spi6>
)

# Four ADS127L11 buses SPI1–4 (quartet epoch); flash: Flash-Stm32CubeOpenOCD.ps1 -Elf .../pat_nucleo_quartet.elf
set(APP_QUARTET_SRC
  "${CMAKE_SOURCE_DIR}/src/main_quartet.c"
  "${CMAKE_SOURCE_DIR}/src/pat_clock.c"
  "${CMAKE_SOURCE_DIR}/src/pat_spi_ads127.c"
  "${CMAKE_SOURCE_DIR}/src/pat_spi_h7_master.c"
  "${CMAKE_SOURCE_DIR}/src/pat_quartet_epoch.c"
  "${CMAKE_SOURCE_DIR}/src/pat_quartet_rolling.c"
  "${CMAKE_SOURCE_DIR}/src/pat_quartet_app.c"
  "${CMAKE_SOURCE_DIR}/src/pat_quartet_spi_irq.c"
  "${CMAKE_SOURCE_DIR}/src/stm32h7xx_hal_msp.c"
  "${CMAKE_SOURCE_DIR}/src/stm32h7xx_it.c"
  "${CMAKE_SOURCE_DIR}/src/ads127l11.c"
)

if(PAT_QUARTET_LINK_SPI6)
  list(APPEND APP_QUARTET_SRC "${CMAKE_SOURCE_DIR}/src/qpd_spi6_slave.c")
endif()

add_executable(pat_nucleo_quartet
  ${CMSIS_SRC}
  ${HAL_SRC}
  ${APP_QUARTET_SRC}
)

target_compile_definitions(pat_nucleo_quartet PRIVATE PAT_QUARTET_PARALLEL_DRDY_WAIT=1)

if(PAT_ADS127_SPI_3WIRE_CS_HELD_LOW)
  target_compile_definitions(pat_nucleo_quartet PRIVATE PAT_ADS127_SPI_3WIRE_CS_HELD_LOW=1)
endif()

if(PAT_QUARTET_DIAG_EPOCH_EVERY)
  target_compile_definitions(pat_nucleo_quartet PRIVATE PAT_QUARTET_DIAG_EPOCH_EVERY=1)
endif()

if(PAT_QUARTET_PARALLEL_SPI_RX3_SEQ_UNLOCKED)
  target_compile_definitions(pat_nucleo_quartet PRIVATE PAT_QUARTET_PARALLEL_SPI_RX3_SEQ_UNLOCKED=1)
endif()

if(PAT_QUARTET_PARALLEL_SPI_REGISTER_MASTER)
  target_compile_definitions(pat_nucleo_quartet PRIVATE PAT_QUARTET_PARALLEL_SPI_REGISTER_MASTER=1)
endif()

if(PAT_QUARTET_LINK_SPI6)
  target_compile_definitions(pat_nucleo_quartet PRIVATE PAT_QUARTET_LINK_SPI6=1)
endif()

if(PAT_SPI123_PRESCALER_DIV STREQUAL "8")
  target_compile_definitions(pat_nucleo_quartet PRIVATE PAT_SPI123_PRESCALER_8=1)
elseif(PAT_SPI123_PRESCALER_DIV STREQUAL "16")
  target_compile_definitions(pat_nucleo_quartet PRIVATE PAT_SPI123_PRESCALER_16=1)
elseif(PAT_SPI123_PRESCALER_DIV STREQUAL "32")
  target_compile_definitions(pat_nucleo_quartet PRIVATE PAT_SPI123_PRESCALER_32=1)
endif()

target_compile_definitions(
  pat_nucleo_quartet
  PRIVATE "PAT_QUARTET_SPI_IRQ_PREEMPT_PRIORITY=${PAT_QUARTET_SPI_IRQ_PREEMPT_PRIORITY}u"
)

target_link_options(pat_nucleo_quartet PRIVATE
  ${CPU_FLAGS}
  -T${LINKER_SCRIPT}
  -Wl,--gc-sections
  -Wl,-Map=$<TARGET_FILE_DIR:pat_nucleo_quartet>/pat_nucleo_quartet.map
  --specs=nano.specs
  --specs=nosys.specs
  -Wl,--print-memory-usage
)

add_custom_command(TARGET pat_nucleo_quartet POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:pat_nucleo_quartet>
          $<TARGET_FILE_DIR:pat_nucleo_quartet>/pat_nucleo_quartet.bin
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:pat_nucleo_quartet>
)

# Single-channel ADS127 workflow on SPI1, then SPI2, SPI3, SPI4 (one active bus per phase).
# Flash: Flash-Stm32CubeOpenOCD.ps1 -Spi1_4  or  -Spi123 (alias)  or  -Elf .../pat_nucleo_spi1_4_scan.elf
set(APP_SPI1_4_SCAN_SRC
  "${CMAKE_SOURCE_DIR}/src/main_spi1_4_scan.c"
  "${CMAKE_SOURCE_DIR}/src/pat_clock.c"
  "${CMAKE_SOURCE_DIR}/src/pat_spi_ads127.c"
  "${CMAKE_SOURCE_DIR}/src/pat_spi_h7_master.c"
  "${CMAKE_SOURCE_DIR}/src/stm32h7xx_hal_msp.c"
  "${CMAKE_SOURCE_DIR}/src/stm32h7xx_it.c"
  "${CMAKE_SOURCE_DIR}/src/ads127l11.c"
)

add_executable(pat_nucleo_spi1_4_scan
  ${CMSIS_SRC}
  ${HAL_SRC}
  ${APP_SPI1_4_SCAN_SRC}
)

target_link_options(pat_nucleo_spi1_4_scan PRIVATE
  ${CPU_FLAGS}
  -T${LINKER_SCRIPT}
  -Wl,--gc-sections
  -Wl,-Map=$<TARGET_FILE_DIR:pat_nucleo_spi1_4_scan>/pat_nucleo_spi1_4_scan.map
  --specs=nano.specs
  --specs=nosys.specs
  -Wl,--print-memory-usage
)

add_custom_command(TARGET pat_nucleo_spi1_4_scan POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:pat_nucleo_spi1_4_scan>
          $<TARGET_FILE_DIR:pat_nucleo_spi1_4_scan>/pat_nucleo_spi1_4_scan.bin
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:pat_nucleo_spi1_4_scan>
)

# One ELF per SPI bus: same app as default single-channel flow (`main.c` for SPI4).
# Flash: Flash-Stm32CubeOpenOCD.ps1 -SingleSpi 1  (..2..3..4)  or  -Elf .../pat_nucleo_spiN_ads127.elf
set(APP_SINGLE_ADS127_SPI_SRC
  "${CMAKE_SOURCE_DIR}/src/main_single_ads127_spi.c"
  "${CMAKE_SOURCE_DIR}/src/pat_clock.c"
  "${CMAKE_SOURCE_DIR}/src/pat_spi_ads127.c"
  "${CMAKE_SOURCE_DIR}/src/pat_spi_h7_master.c"
  "${CMAKE_SOURCE_DIR}/src/stm32h7xx_hal_msp.c"
  "${CMAKE_SOURCE_DIR}/src/stm32h7xx_it.c"
  "${CMAKE_SOURCE_DIR}/src/ads127l11.c"
)

foreach(bus RANGE 1 4)
  set(tgt "pat_nucleo_spi${bus}_ads127")
  add_executable(${tgt}
    ${CMSIS_SRC}
    ${HAL_SRC}
    ${APP_SINGLE_ADS127_SPI_SRC}
  )
  target_compile_definitions(${tgt} PRIVATE PAT_ADS127_SINGLE_SPI_BUS=${bus})
  if(bus EQUAL 2 AND PAT_SPI2_PC2SO_OPEN_INSTEAD)
    target_compile_definitions(${tgt} PRIVATE PAT_SPI2_PC2SO_OPEN_INSTEAD=1)
  endif()
  target_link_options(${tgt} PRIVATE
    ${CPU_FLAGS}
    -T${LINKER_SCRIPT}
    -Wl,--gc-sections
    -Wl,-Map=$<TARGET_FILE_DIR:${tgt}>/${tgt}.map
    --specs=nano.specs
    --specs=nosys.specs
    -Wl,--print-memory-usage
  )
  add_custom_command(TARGET ${tgt} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${tgt}>
            $<TARGET_FILE_DIR:${tgt}>/${tgt}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${tgt}>
  )
endforeach()

if(PAT_ADS127_STRICT_BRINGUP)
  target_compile_definitions(pat_nucleo_h753 PRIVATE PAT_ADS127_STRICT_BRINGUP=1)
  target_compile_definitions(pat_nucleo_quartet PRIVATE PAT_ADS127_STRICT_BRINGUP=1)
  target_compile_definitions(pat_nucleo_spi1_4_scan PRIVATE PAT_ADS127_STRICT_BRINGUP=1)
  foreach(bus RANGE 1 4)
    target_compile_definitions(pat_nucleo_spi${bus}_ads127 PRIVATE PAT_ADS127_STRICT_BRINGUP=1)
  endforeach()
endif()

if(PAT_ADS127_SPI_HAL_LEGACY)
  target_compile_definitions(pat_nucleo_h753 PRIVATE PAT_ADS127_SPI_HAL_LEGACY=1)
  target_compile_definitions(pat_nucleo_quartet PRIVATE PAT_ADS127_SPI_HAL_LEGACY=1)
  target_compile_definitions(pat_nucleo_spi1_4_scan PRIVATE PAT_ADS127_SPI_HAL_LEGACY=1)
  foreach(bus RANGE 1 4)
    target_compile_definitions(pat_nucleo_spi${bus}_ads127 PRIVATE PAT_ADS127_SPI_HAL_LEGACY=1)
  endforeach()
endif()

# SPI5 MEMS mirror + UART5/7 command path bring-up target.
set(APP_MEMS_SRC
  "${CMAKE_SOURCE_DIR}/src/main_mems_bringup.c"
  "${CMAKE_SOURCE_DIR}/src/pat_clock.c"
  "${CMAKE_SOURCE_DIR}/src/stm32h7xx_hal_msp.c"
  "${CMAKE_SOURCE_DIR}/src/stm32h7xx_it.c"
  "${CMAKE_SOURCE_DIR}/src/ad5664r.c"
  "${CMAKE_SOURCE_DIR}/src/pat_mems_regs.c"
  "${CMAKE_SOURCE_DIR}/src/pat_mems_sm.c"
  "${CMAKE_SOURCE_DIR}/src/pat_crc32.c"
  "${CMAKE_SOURCE_DIR}/src/pat_uart5_pat5.c"
  "${CMAKE_SOURCE_DIR}/src/pat_uart7_laser.c"
)

add_executable(pat_nucleo_mems_bringup
  ${CMSIS_SRC}
  ${HAL_SRC}
  ${APP_MEMS_SRC}
)

target_link_options(pat_nucleo_mems_bringup PRIVATE
  ${CPU_FLAGS}
  -T${LINKER_SCRIPT}
  -Wl,--gc-sections
  -Wl,-Map=$<TARGET_FILE_DIR:pat_nucleo_mems_bringup>/pat_nucleo_mems_bringup.map
  --specs=nano.specs
  --specs=nosys.specs
  -Wl,--print-memory-usage
)

add_custom_command(TARGET pat_nucleo_mems_bringup POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:pat_nucleo_mems_bringup>
          $<TARGET_FILE_DIR:pat_nucleo_mems_bringup>/pat_nucleo_mems_bringup.bin
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:pat_nucleo_mems_bringup>
)
