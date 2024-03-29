# CMakeLists for the arc2040podule firmware project
#
# MIT License
#
# Copyright (c) 2021 Matt Evans
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#
#

cmake_minimum_required(VERSION 3.13)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(arc2040podule)

set(PICO_BOARD 2040podule)
set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_SOURCE_DIR})

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
message(STATUS "Included Relocatable Module list: ${PODULE_MODULES}")

if (NOT BOARD_HW)
  set(BOARD_HW "2")
else()
  set(BOARD_HW "${BOARD_HW}")
endif()
unset(BOARD_HW CACHE)
message(STATUS "Board HW rev: ${BOARD_HW}")

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

add_custom_target(mod_pipe_build
  COMMAND echo "Building mod_pipe" && make
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/mod_pipe
)

set_target_properties(mod_pipe_build
  PROPERTIES
  ADDITIONAL_CLEAN_FILES "${CMAKE_CURRENT_SOURCE_DIR}/mod_pipe/module.elf;${CMAKE_CURRENT_SOURCE_DIR}/mod_pipe/module"
  )

if (TARGET tinyusb_device)
  add_executable(firmware
    arc_pipe_podule.c
    podule_interface.c
    payload.S
    usb_descriptors.c
    pipe_packet.c
    utils.c
    )
  add_dependencies(firmware payload_build)
  add_dependencies(payload_build loader_build)
  add_dependencies(payload_build mod_pipe_build)
  add_compile_definitions(BOARD_HW=${BOARD_HW})

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

