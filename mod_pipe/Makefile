
VERBOSE ?= 1
ACC = arm-none-eabi-gcc
AOC = arm-none-eabi-objcopy
ACFLAGS = -mcpu=arm7tdmi -ffreestanding -Wl,--gc-sections -nostdlib -Wl,-Ttext=0

BUILD_DATE=$(shell date)

ACFLAGS += -DBUILD_DATE="\"$(BUILD_DATE)\""

ifeq ($(VERBOSE), 1)
	ACFLAGS += -DVERBOSE
endif

all:	module

module.elf:	module.S
	$(ACC) $(ACFLAGS) $< -o $@

module:	module.elf
	$(AOC) -O binary $< $@

clean:
	rm -f module module.elf *~
