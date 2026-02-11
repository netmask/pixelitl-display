# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "")
  file(REMOVE_RECURSE
  "bootloader/bootloader.bin"
  "bootloader/bootloader.elf"
  "bootloader/bootloader.map"
  "clock.wasm.S"
  "config/sdkconfig.cmake"
  "config/sdkconfig.h"
  "demo.wasm.S"
  "esp-idf/mbedtls/x509_crt_bundle"
  "flash_app_args"
  "flash_bootloader_args"
  "flash_project_args"
  "flasher_args.json"
  "flasher_args.json.in"
  "ldgen_libraries"
  "ldgen_libraries.in"
  "pixelitl_display.bin"
  "pixelitl_display.map"
  "plasma.wasm.S"
  "project_elf_src_esp32s3.c"
  "x509_crt_bundle.S"
  )
endif()
