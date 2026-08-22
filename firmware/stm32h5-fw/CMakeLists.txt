cmake_minimum_required(VERSION 3.13)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/tools")
include(arm-none-eabi-gcc)


# 프로젝트 이름은 최종 출력 파일 이름으로 사용된다.
#
set(PRJ_NAME stm32h5-w6300-fw)


project(${PRJ_NAME}
  LANGUAGES ASM C CXX
)


set(EXECUTABLE ${PRJ_NAME}.elf)



# 지정한 폴더에 있는 파일만 포함한다.
#
file(GLOB SRC_FILES CONFIGURE_DEPENDS
  src/*.cpp
  src/*.c
  src/ap/*.cpp
  src/ap/*.c
  src/bsp/*.c
  src/bsp/rtos/*.c  
  src/bsp/device/*.c

  # USB
  src/lib/ST/STM32_USB_Device_Library/Core/Src/*.c    
)

# 하위폴더에 있는 파일까지 포함한다.
#
file(GLOB_RECURSE SRC_FILES_RECURSE CONFIGURE_DEPENDS
  src/ap/*.cpp
  src/ap/*.c
  src/bsp/*.s
  src/common/*.c
  src/hw/*.c
  # src/lib/ST/STM32H5xx_HAL_Driver/Src/*.c
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



target_include_directories(${EXECUTABLE} PRIVATE 
  src  
  src/ap
  src/ap/modules
  src/ap/modules/common
  src/bsp
  src/bsp/device
  src/bsp/rtos  
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
  src/hw/driver/usb/usb_cdc
  src/hw/driver/usb/usb_msc
  src/hw/driver/usb/usb_hid
  src/hw/driver/usb/usb_cmp
  src/ap/modules/cmd
  src/lib/ST/STM32_USB_Device_Library/Core/Inc    
)

target_compile_definitions(${EXECUTABLE} PRIVATE
  -DSTM32H563xx
  -DUSE_HAL_DRIVER  
  -DLFS_THREADSAFE
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
  # -Wenum-conversion

  -Wall
  -g3
  -O0
  )

set_source_files_properties(
    ${SRC_FILES_SPECIAL}
    PROPERTIES
        COMPILE_OPTIONS "-O0"
)

target_link_options(${EXECUTABLE} PRIVATE
  -T../src/bsp/ldscript/STM32H563xx_FLASH.ld

  -mcpu=cortex-m33
  -mthumb
  -mfpu=fpv4-sp-d16
  -mfloat-abi=hard
  -specs=nano.specs
  -lc
  -lm
  -lstdc++
  -lsupc++
  -fno-rtti
  
  -Wl,-Map=${PRJ_NAME}.map,--cref
  -Wl,--gc-sections
  -Xlinker -print-memory-usage -Xlinker
  )

set (CMAKE_C_FLAGS "-Wenum-conversion")
set (CMAKE_CXX_FLAGS "-fno-rtti")

message(${CMAKE_CXX_COMPILER_VERSION})
if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 11)
  string(APPEND CMAKE_EXE_LINKER_FLAGS "-Wl,--no-warn-rwx-segments")
endif()

add_custom_command(TARGET ${EXECUTABLE} 
  POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} ARGS -O binary ${EXECUTABLE} ${PROJECT_NAME}.bin
  COMMENT "Invoking: Make Bin"
  )  


# 부트로더가 받을 .uf2 를 함께 만든다.
#
#   --base 0x0 를 반드시 명시한다. uf2conv.py 의 기본 appstartaddr 는 0x2000 이라
#   생략하면 8KB 오프셋 구멍이 생긴다. targetAddr 는 0-베이스 오프셋이고
#   부트로더가 여기에 (슬롯 베이스 + FLASH_SIZE_TAG) 를 더한다.
#
#   버전은 configure 시점에 읽는다. hw_def.h 를 고쳐도 cmake 를 다시 돌리지 않으면
#   파일 이름이 옛 버전으로 남는다. 실제로 그래서 rm -rf build 를 해야 했다.
#   이 줄이 hw_def.h 가 바뀌면 configure 를 자동으로 다시 돌게 한다.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${CMAKE_CURRENT_SOURCE_DIR}/src/hw/hw_def.h")

file(READ "src/hw/hw_def.h" version_in)
string(REPLACE "\"" "" version_out ${version_in})
string(REGEX MATCH "_DEF_FIRMWATRE_VERSION * ([-A-Za-z0-9]*)" _ ${version_out})
set(FIRM_VER ${CMAKE_MATCH_1})

find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  add_custom_command(TARGET ${EXECUTABLE}
    POST_BUILD
    COMMAND ${Python3_EXECUTABLE} ARGS ${CMAKE_CURRENT_SOURCE_DIR}/tools/uf2/uf2conv.py
            ${PROJECT_NAME}.bin --base 0x0 --family 0xFFFF0003
            --convert --output ${PRJ_NAME}-${FIRM_VER}.uf2
    COMMENT "Invoking: Make UF2"
    )
endif()
  

