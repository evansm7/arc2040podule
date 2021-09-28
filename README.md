# Arc Pipe Podule firmware

v0.1

28 September 2021

## This isn't real docs...

(More notes to self, currently)

This is firmware for the 2040 Arc Pipe Podule. (TBD: Link to PCB design repository.)

It provides the read/write bus bitbanging infrastructure, and a corresponding RISC OS loader to provide a large expansion ROM space (limited by the podule's SPI flash, usually 2MB-8MB).  This works reliably.

This project also includes a _proof-of-concept_ USB communications pipe, permitting the Arc to make requests over USB to the host (a server running on Linux).  This is very much WIP; the design is being tinkered with and is evolving, particularly the protocols and services.


# Build setup

> mkdir build && cd build
> PICO_SDK_PATH=~/src/pico-sdk/ PICO_TOOLCHAIN_PATH=/usr/local/gcc-arm-embedded \
    PICO_BOARD=2040podule PICO_BOARD_HEADER_DIRS=~/code/arc2040podule/ cmake .. -DPODULE_MODULES=./mod_pipe/module

Add on other modules by extending `PODULE_MODULES` (e.g. `PODULE_MODULES="./mod_pipe/module /path/thingy,ffa"`).  They're automatically loaded by RISC OS.

## Sub-projects

   * loader:  RISC OS module loader code
   * mod_pipe:  RISC OS frontend for the USB communication pipe
   * server:  Linux server for the `ttyACMx` host-side of the USB pipe



# Copyright/Licence

Copyright 2021 Matt Evans

Licence TBD!
