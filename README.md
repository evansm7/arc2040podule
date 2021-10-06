# Acorn Archimedes Pipe Podule firmware

v0.1

5 October 2021

This repository contains firmware for the _Arc Pipe Podule_. (TBD: Link to PCB design repository.)  This podule is based on the Raspberry Pi RP2040 MCU, and firmware builds using the `pico-sdk`.  The podule contains 2MB+ of flash, and has a USB device port that can be used for comms to a host.

The firmware provides the read/write bus bitbanging infrastructure, and presents a corresponding RISC OS loader to provide a large expansion ROM space (limited by the podule's SPI flash, usually 2MB-8MB).  This works reliably.

This project also includes a _proof-of-concept_ USB communications pipe, permitting the Arc to make requests over USB to the host (a server running on Linux).  This is very much WIP; the design is being tinkered with and is evolving, particularly the protocols and services.  This comprises a RISC OS Relocatable Module (called *mod_pipe*) and a Linux host server program (imaginitively, *server*).

What's this useful for?  Well, at a minimum and ignoring USB, it's an easily-reflashable ROM card that can be used to provide filesystems/drivers/files at boot.

In addition, the ultimate idea is the USB device port (in conjunction with a host-side server) can provide file transfer capabilities.  This makes bootstrapping (or cross-development for) an Archimedes much easier.


# Building

## Sub-projects

There are multiple moving parts:

   * *firmware*:  RP2040 firmware that runs on the podule itself
   * *loader*:  RISC OS module loader code
   * *mod_pipe*:  RISC OS Relocatable Module, a frontend for the USB communication pipe
   * *server*:  Linux server for the `ttyACMx` host-side of the USB pipe

The firwmare embeds the loader and all Relocatable Modules (including *mod_pipe*).  To the Archimedes, the podule presents a loader and a chunk directory, which loads the given modules.

## Building firmware:  Cmake/pico-sdk

(Commands are relative to the root of this repo.)

You will need `pico-sdk` checked out somewhere.  The sub-projects expect `arm-none-eabi-gcc` (and objdump) in your `$PATH`.  Sorry.

Make a build subdirectory, and point Cmake at the `pico-sdk`/toolchain as follows:

```
$ mkdir build && cd build
$ PICO_SDK_PATH=~/src/pico-sdk/ \
  PICO_TOOLCHAIN_PATH=/usr/local/gcc-arm-embedded \
  cmake .. -DPODULE_MODULES=./mod_pipe/module
$ make firmware
```

The (hacky af) Makefile then builds the *loader*, *mod_pipe*, creates RISC OS podule chunk directories, and assembles a ROM payload.  Finally, it builds the podule firmware.

Note the `PODULE_MODULES` variable:  this includes the *mod_pipe* module in the RP2040 firmware as a payload sent to the Archimedes.

Add on other modules by extending `PODULE_MODULES` with semicolons (e.g. `PODULE_MODULES="./mod_pipe/module;/path/thingy,ffa"`).


## Building server

The Linux-side server is built separately:

```
$ (cd server && make)
```

## Flash to podule

   * Hold down `BOOT` and reset the podule to enter USB programming mode
   * Copy the `build/firmware.uf2` file to the USB MSD that appears.  It'll have the name `RPI-RP2`.

The firmware will flash, and the podule will automatically reset the Archimedes.  Hold down 'Ctrl' when you do this, and the Arc will re-load any newly-updated modules.

## Caveats

I'm a complete Cmake n00b, and the `CMakeLists.txt` rules are pretty shonky.  The *mod_pipe* and *loader* sub-projects are embedded as external make-based projects, and the dependencies don't work properly.  A fresh build works, but the top-level Makefile won't notice edits to these sub-projects.

If you're hacking on *mod_pipe* or *loader*, it's worth doing a `make clean firmware`.

# Running/operation

The server runs on the host:  it is highly unconfigurable, and highly hacky/WIP/PoC.

Run from a directory containing files to share:

```
cd /some/path/with/files/to/share
path/to/server
```

The server opens `/dev/ttyACM0`.  This obvs should be configurable, but is currently hardwired.  It waits for requests from the podule.


# Usage

With podule connected, server running, on the Arc some new commands are provided.

For example, 'pipe info' returns host/server information (a kind of 'ping'):

```
*PI
01:00000024:00000001:ArcPipePodule host server
```

Or, copy a file to local FS:

```
*PCPL file-on-linux localfile
```

# Technical details

The ArcPipePodule appears to be a 4KB memory (one byte per word of a 16KB address space).

The memory is split into the following regions:

   * 0-1023:  Podule header, initial chunk directory, and podule loader
   * 1024-2047:  Paged/banked window onto "podule ROM space"
   * 2048-4095:  "Registers"

The ROM banked access is performed by a handshake between the loader and the podule firmware:  A bank index is written with top bit set, the podule makes the relevant ROM page visible, and the index top bit is cleared showing the loader it's safe to proceed to read the ROM data.


## USB communication pipe

The USB comms protocol is a bit simplistic.  The podule provides reliable transport for packets from 1-512 bytes in size.  It is driven entirely from the Arc, sending packets to the host for service and waiting for a response.  Transmit means Arc to host, Receive means host to Arc.

The packets are sent on "channels"; the idea is different services use different channels, e.g. host info and file copying.

There's a vague nod to TX/RX queueing and multiple outstanding buffers, but that isn't supported yet.  There are TX/RX descriptor queues (currently only 2-entry), but the descriptors point to TX/RX buffers of which there can be only one outstanding currently.

The descriptors and buffers are held in the "Registers" region; TX/RX form a pair of producer/consumer queues either read or written by the Arc or podule.  The descriptors contain a "READY" bit, which means a new packet was received (and consumed by the Arc) or produced by the Arc (and transmitted by the podule).  The producer sets the ready bit and the consumer clears it.

The podule translates between a packet in the podule address space and a USB CDC ACM connection.  The payload is wrapped with a small header indicating the Channel ID (CID) and payload size.

For the transmit-to-host path, the Linux server simply reads bytes from the "serial port", reassembles into the wrapped packet, then breaks it up into a CID/size and a payload which is passed to a channel handler.  The channel handler parses the message, and might then return data/a response.  For receive, the reverse occurs (data produced by the server is wrapped, sent to the ACM device, unwrapped on the podule and placed in an RX buffer).

Interrupts (for example, on RX) are not supported yet (but are supported by the podule hardware).


## Bugs and issues

Informally:

   * Fix Cmake dependencies with sub-projects
   * Implement allocator & support multiple outstanding TX/RX buffers
   * The RISC OS module is _very hacky_.  It doesn't include particularly thorough error checking.  Filename size and manipulation should be improved.
   * The server is similarly hacky and should be much more robust.
   * Implement RX-ready and TX-has-space IRQs to the Archimedes.  This would permit the client module to sleep/do something non-polled.
   * Features features features.


# Future ideas

My wishlist:

   * Extend the raw file copying utilities to provide disc imaging.  For example, write a remote file's data to a FD/HD, or read a FD/HD and write to the host.
   * Over the USB comms channel, provide a Filecore-based remote disc image filing system:  i.e., mount a `.hdf` on your Linux machine via the USB link.
   * Implement a RISCiX block driver for same.
   * A HostFS-derived remote filesystem!  A RISC OS module provides a filesystem whose fs_ops are bundled into requests to the host.
   * Use another CID for Ethernet:  present a DCI4-compliant Ethernet driver to the Arc.
   * Or, move away from the USB comms link and instead use USB CDC Ethernet to present a genuine Ethernet device (which might then be easily shared using DHCP/NAT etc. from a Mac/Win machine).
   * USB host:  Could use USB Mass Storage Devices (somewhat slowly) and present as a FileCore FS
   * On the 3 spare GPIOs, add an SD card (present as a storage device/FS).


# Copyright/License

 MIT License

 Copyright (c) 2021 Matt Evans

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.

