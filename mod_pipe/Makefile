
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
