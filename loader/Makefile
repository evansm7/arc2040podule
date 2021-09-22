
VERBOSE ?= 1
ACC = arm-none-eabi-gcc
AOC = arm-none-eabi-objcopy
ACFLAGS = -mcpu=arm7tdmi -ffreestanding -Wl,--gc-sections -nostdlib -Wl,-Ttext=0

ifeq ($(VERBOSE), 1)
	ACFLAGS += -DVERBOSE
endif

all:	loader.bin

loader.elf:	loader.S
	$(ACC) $(ACFLAGS) $< -o $@

%.bin:	%.elf
	$(AOC) -O binary $< $@

clean:
	rm -f loader.bin loader.elf *~
