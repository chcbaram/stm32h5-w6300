cmake_minimum_required(VERSION 3.13)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/tools")
include(arm-none-eabi-gcc)

# TinyUSB 의 tinyusb_target_add() 함수 정의를 로드한다. (6단계에서 사용)
include("src/lib/tinyusb/src/CMakeLists.txt")


# 프로젝트 이름은 최종 출력 파일 이름으로 사용된다.
#
set(PRJ_NAME stm32h5-w6300-boot)


project(${PRJ_NAME}
  LANGUAGES ASM C
)


set(EXECUTABLE ${PRJ_NAME}.elf)



# 지정한 폴더에 있는 파일만 포함한다.
#
file(GLOB SRC_FILES CONFIGURE_DEPENDS
  src/*.c
  src/ap/*.c
  src/bsp/*.c
  src/bsp/device/*.c

  # TinyUSB DCD 포트 (STM32H5 는 stm32_fsdev 계열)
  src/lib/tinyusb/src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c
)

# 하위폴더에 있는 파일까지 포함한다.
#
file(GLOB_RECURSE SRC_FILES_RECURSE CONFIGURE_DEPENDS
  src/ap/*.c
  src/bsp/*.s
  src/common/*.c
  src/hw/*.c
  )


file(GLOB_RECURSE SRC_FILES_SPECIAL CONFIGURE_DEPENDS
  src/lib/ST/STM32H5xx_HAL_Driver/Src/*.c
  )


# 특정 폴더를 빌드에서 제외한다.
#
set(EXCLUDE_PATHS
)

foreach(TMP_PATH ${SRC_FILES_RECURSE})
  foreach(EXCLUDE_PATH ${EXCLUDE_PATHS})
    string(FIND ${TMP_PATH} ${EXCLUDE_PATH} RESULT)
    if(NOT ${RESULT} EQUAL -1)
      list(REMOVE_ITEM SRC_FILES_RECURSE ${TMP_PATH})
    endif()
  endforeach(EXCLUDE_PATH)
endforeach(TMP_PATH)


add_executable(${EXECUTABLE}
  ${SRC_FILES}
  ${SRC_FILES_RECURSE}
  ${SRC_FILES_SPECIAL}
)

tinyusb_target_add(${EXECUTABLE})


target_include_directories(${EXECUTABLE} PRIVATE
  src
  src/ap
  src/ap/modules
  src/ap/modules/common
  src/bsp
  src/bsp/device
  src/common
  src/common/core
  src/common/hw/include
  src/hw
  src/hw/driver
  src/lib

  src/lib/ST/CMSIS/Include
  src/lib/ST/CMSIS/Device/ST/STM32H5xx/Include
  src/lib/ST/STM32H5xx_HAL_Driver/Inc

  # USB
  src/hw/driver/usb
)

target_compile_definitions(${EXECUTABLE} PRIVATE
  -DSTM32H563xx
  -DUSE_HAL_DRIVER
  )

target_compile_options(${EXECUTABLE} PRIVATE
  -mcpu=cortex-m33
  -mthumb
  -mfpu=fpv4-sp-d16
  -mfloat-abi=hard

  -fdata-sections
  -ffunction-sections

  -fstack-usage
  -Wmissing-field-initializers
  -Wuninitialized

  -Wall
  -g3
  -Os
  )

target_link_options(${EXECUTABLE} PRIVATE
  -T../src/bsp/ldscript/STM32H563xx_BOOT.ld

  -mcpu=cortex-m33
  -mthumb
  -mfpu=fpv4-sp-d16
  -mfloat-abi=hard
  -specs=nano.specs
  -lc
  -lm

  -Wl,-Map=${PRJ_NAME}.map,--cref
  -Wl,--gc-sections
  -Xlinker -print-memory-usage -Xlinker
  )

set (CMAKE_C_FLAGS "-Wenum-conversion")

if(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 11)
  string(APPEND CMAKE_EXE_LINKER_FLAGS "-Wl,--no-warn-rwx-segments")
endif()

add_custom_command(TARGET ${EXECUTABLE}
  POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} ARGS -O binary ${EXECUTABLE} ${PROJECT_NAME}.bin
  COMMENT "Invoking: Make Bin"
  )
