cmake_minimum_required(VERSION 3.20)
project(pat_nucleo_h753 C ASM)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
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
