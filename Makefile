APPNAME := code

CFLAGS = -mcpu=cortex-a8 -g
OPTIMIZATION_FLAGS= -O2
TOOLCHAIN_PREFIX = 
OBJDIR := obj
SRCDIR := src

BBB_SOURCES := $(wildcard $(SRCDIR)/*.s) $(wildcard $(SRCDIR)/*.c) $(wildcard $(SRCDIR)/emu68/*.c)
BBB_OBJECTS := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o, \
    $(patsubst $(SRCDIR)/emu68/%.c,$(OBJDIR)/%.o, \
    $(patsubst $(SRCDIR)/%.s,$(OBJDIR)/%.o, \
    $(BBB_SOURCES))))

DEFINES :=
COMMON_FLAGS := $(EXTRA_FLAGS) -Iinc -mbig-endian -ffixed-r11 -marm -mcpu=cortex-a8 -Os \
	-mfpu=neon-vfpv4 -pedantic -pedantic-errors -fomit-frame-pointer \
	-Wall -Wextra -Werror -nostdlib -ffreestanding -g -fmax-errors=5
#-mbig-endian 
#-fno-builtin

CFLAGS := $(COMMON_FLAGS) $(DEFINES) -std=gnu11
CXXFLAGS:= $(COMMON_FLAGS) $(DEFINES) -std=c++11
LDFLAGS := -static -nostdlib -nostartfiles -T linker.lds \
	-Wl,--Map=$(APPNAME).map \
	-Wl,--build-id \
	-Wl,--be8 \
	-Wl,--format=elf32-bigarm
	
.PHONY: all premake clean

all: premake $(BBB_OBJECTS)
	$(TOOLCHAIN_PREFIX)gcc $(BBB_OBJECTS) -o $(APPNAME).elf $(LDFLAGS)
	$(TOOLCHAIN_PREFIX)objcopy --gap-fill=0xff -O binary $(APPNAME).elf $(APPNAME).bin

premake:
	@mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR)
	rm -f $(APPNAME).elf
	rm -f $(APPNAME).bin

%.o: ../$(SRCDIR)/%.c
	$(TOOLCHAIN_PREFIX)gcc $(CFLAGS) -c $< -o $@

%.o: ../$(SRCDIR)/emu68/%.c
	$(TOOLCHAIN_PREFIX)gcc $(CFLAGS) -c $< -o $@

%.o: ../$(SRCDIR)/%.s
	$(TOOLCHAIN_PREFIX)as -mcpu=cortex-a8 -mbig-endian -c $< -o $@