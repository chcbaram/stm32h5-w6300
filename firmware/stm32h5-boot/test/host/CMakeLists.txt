cmake_minimum_required(VERSION 3.13)
project(boot_host_test LANGUAGES C)

set(CMAKE_C_STANDARD 11)

set(BOOT_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../src)

add_executable(boot_test
  main.c
  test_util.c
  test_slot_logic.c
  test_apply.c
  test_bootlog.c
  test_powerloss.c

  mock/mock_flash.c
  mock/mock_env.c

  # 실제 부트로더 코드. 목 플래시 위에서 그대로 돈다.
  ${BOOT_SRC}/ap/modules/boot/boot.c
  ${BOOT_SRC}/ap/modules/boot/boot_log.c
  ${BOOT_SRC}/common/core/util_core.c
)

# mock 이 실제 헤더보다 앞에 와야 shim 이 적용된다.
target_include_directories(boot_test PRIVATE
  mock
  .
  ${BOOT_SRC}/ap/modules
  ${BOOT_SRC}/common
  ${BOOT_SRC}/common/core
  ${BOOT_SRC}/common/hw/include
  ${BOOT_SRC}/hw
)

target_compile_options(boot_test PRIVATE
  -Wall -Wextra -Wno-unused-parameter -g -O1
  -fsanitize=address,undefined
)
target_link_options(boot_test PRIVATE -fsanitize=address,undefined)
