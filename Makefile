APPNAME := code

TOOLCHAIN_PREFIX = arm-none-eabi-

OBJDIR := obj
SRCDIR := src

BBB_SOURCES := $(wildcard $(SRCDIR)/*.s) $(wildcard $(SRCDIR)/*.c) $(wildcard $(SRCDIR)/emu68/*.c)
BBB_OBJECTS := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o, \
    $(patsubst $(SRCDIR)/emu68/%.c,$(OBJDIR)/%.o, \
    $(patsubst $(SRCDIR)/%.s,$(OBJDIR)/%.o, \
    $(BBB_SOURCES))))

DEFINES :=
CCFLAGS_CPU := -Iinc  \
	-fno-exceptions -fno-unwind-tables -nostdlib -fno-common \
	-marm -mcpu=cortex-a8 -mfpu=vfpv3 -mfloat-abi=hard \
	-ffunction-sections -fdata-sections -fomit-frame-pointer \
	-mbig-endian -ffreestanding -mbe8 \
	-std=gnu11 -MMD -fmax-errors=5 

# Universally good compiler warnings	
CCFLAGS_WARN := \
	-Wall -Wshadow -Wdouble-promotion -Wformat-overflow -Wformat-truncation \
	-Wundef -Wno-unused-parameter

# Optimize for debugging, but include basic optimizers for not-terrible-code
CCFLAGS_OPT := -Og -g3 -ggdb -D_DEBUG

COMMON_FLAGS := $(CCFLAGS_WARN) $(CCFLAGS_CPU) $(CCFLAGS_OPT)

# COMMON_FLAGS
#  $(EXTRA_FLAGS) -Iinc -mbig-endian -marm -mcpu=cortex-a8 \
# 	-mfpu=neon-vfpv3 -mfloat-abi=hard -fomit-frame-pointer -Og -g3 -ggdb

# -pedantic -pedantic-errors -Wall -Wextra -Werror -nostdlib -ffreestanding -g -fmax-errors=5
#-mbig-endian 
#-fno-builtin

CFLAGS := $(COMMON_FLAGS) $(DEFINES) -std=gnu11
CXXFLAGS:= $(COMMON_FLAGS) $(DEFINES) -std=c++11
LDFLAGS := -static -nostdlib -nostartfiles -T linker.lds \
	-Wl,--Map=$(APPNAME).map -Wl,--gc-sections \
	-Wl,--be8 -Wl,--format=elf32-bigarm \

.PHONY: all premake clean

all: premake $(BBB_OBJECTS)
	$(TOOLCHAIN_PREFIX)gcc $(BBB_OBJECTS) -o $(APPNAME).elf $(LDFLAGS)

premake:
	@mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR)
	rm -f $(APPNAME).elf
	rm -f $(APPNAME).bin

%.o: ../$(SRCDIR)/%.c
	$(TOOLCHAIN_PREFIX)gcc $(CFLAGS) -S -c $< -o $(@:%.o=%.s) &
	$(TOOLCHAIN_PREFIX)gcc $(CFLAGS) -c $< -o $@

%.o: ../$(SRCDIR)/emu68/%.c
	$(TOOLCHAIN_PREFIX)gcc $(CFLAGS) -c $< -o $@

%.o: ../$(SRCDIR)/%.s
	$(TOOLCHAIN_PREFIX)as -mcpu=cortex-a8 -mbig-endian -c $< -o $@