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

VERBOSE ?= 1
DEBUG ?= 0

ACC = arm-none-eabi-gcc
AOC = arm-none-eabi-objcopy
ACFLAGS = -mcpu=arm7tdmi -ffreestanding -Wl,--gc-sections -nostdlib -Wl,-Ttext=0

BUILD_DATE=$(shell date)

ACFLAGS += -DBUILD_DATE="\"$(BUILD_DATE)\""
ACFLAGS += -DDEBUG=$(DEBUG)

ifeq ($(VERBOSE), 1)
	ACFLAGS += -DVERBOSE
endif

SOURCES = module.S
SOURCES += utils.S
SOURCES += pipe_packet.S
SOURCES += commands.S
SOURCES += commands_rawfile.S


all:	module

module.elf:	$(SOURCES)
	$(ACC) $(ACFLAGS) $^ -o $@

module:	module.elf
	$(AOC) -O binary $< $@

clean:
	rm -f module module.elf *~
