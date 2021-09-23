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

if (TARGET tinyusb_device)
  add_executable(firmware
    arc_pipe_podule.c
    podule_interface.c
    payload.S
  )

  target_link_libraries(firmware pico_stdlib pico_multicore)
  # enable usb output, disable uart output
  pico_enable_stdio_usb(firmware 1)
  pico_enable_stdio_uart(firmware 0)
  pico_add_extra_outputs(firmware)
elseif(PICO_ON_DEVICE)
   message(WARNING "not building firmware because TinyUSB submodule is not initialized in the SDK")
endif()
