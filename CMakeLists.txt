cmake_minimum_required(VERSION 3.13)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(arc2040podule)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# Target 1:

if (TARGET tinyusb_device)
  add_executable(test_conns
  	test_conns.c
  )

  target_link_libraries(test_conns pico_stdlib)
  # enable usb output, disable uart output
  pico_enable_stdio_usb(test_conns 1)
  pico_enable_stdio_uart(test_conns 0)
  pico_add_extra_outputs(test_conns)
elseif(PICO_ON_DEVICE)
   message(WARNING "not building test_conns because TinyUSB submodule is not initialized in the SDK")
endif()

# Target 2:

set(PODULE_MODULES "${PODULE_MODULES}" CACHE STRING "Space-separated list of Relocatable Modules to include in the ROM image" FORCE)
message(STATUS "Module list: ${PODULE_MODULES}")

# This is pretty hacky (ME doesn't know cmake).  Ideas for improvement are to
# track the inputs (loader, PODULE_MODULES) as explicit deps, and make these
# build commands execute only if necessary.
add_custom_target(payload_build
  COMMAND echo "Building podule header"
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tools/mk_chunk_dir.py -H -d "ArcPipePodule" -s "0001" -l ${CMAKE_CURRENT_SOURCE_DIR}/loader/loader.bin -o ${CMAKE_BINARY_DIR}/payload_podule_header.bin
  COMMAND echo "Building podule ROM"
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tools/mk_chunk_dir.py -o ${CMAKE_BINARY_DIR}/payload_podule_rom.bin ${PODULE_MODULES}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
  )

set_target_properties(payload_build
  PROPERTIES
  ADDITIONAL_CLEAN_FILES "${CMAKE_BINARY_DIR}/payload_podule_header.bin;${CMAKE_BINARY_DIR}/payload_podule_rom.bin"
  )

add_custom_target(loader_build
  COMMAND echo "Building loader" && make
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/loader
)

set_target_properties(loader_build
  PROPERTIES
  ADDITIONAL_CLEAN_FILES "${CMAKE_CURRENT_SOURCE_DIR}/loader/loader.elf;${CMAKE_CURRENT_SOURCE_DIR}/loader/loader.bin"
  )

if (TARGET tinyusb_device)
  add_executable(firmware
    arc_pipe_podule.c
    podule_interface.c
    payload.S
    usb_descriptors.c
    )
  add_dependencies(firmware payload_build)
  add_dependencies(payload_build loader_build)

  message("Adding ${PICO_SDK_PATH}/lib/tinyusb/src to include path")
  target_include_directories(firmware PRIVATE ${PICO_SDK_PATH}/lib/tinyusb/src)
  target_include_directories(firmware PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/)

  target_link_libraries(firmware
    pico_stdlib
    pico_multicore
    tinyusb_device_unmarked
    )
  pico_enable_stdio_uart(firmware 1)
  pico_add_extra_outputs(firmware)

elseif(PICO_ON_DEVICE)
   message(WARNING "not building firmware because TinyUSB submodule is not initialized in the SDK")
endif()

