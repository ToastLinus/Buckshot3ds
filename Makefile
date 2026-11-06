include $(DEVKITARM)/3ds_rules

CTRULIB := $(DEVKITPRO)/libctru
ARCH    := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS  := -g -Wall -O2 -mword-relocations -ffunction-sections $(ARCH) -D__3DS__ -I$(CTRULIB)/include
LDFLAGS := -specs=3dsx.specs -g $(ARCH) -L$(CTRULIB)/lib

all: buckshot.3dsx

buckshot.elf: source/main.c
	arm-none-eabi-gcc $(CFLAGS) $< $(LDFLAGS) -lcitro2d -lcitro3d -lctru -lm -o $@

buckshot.3dsx: buckshot.elf
	3dsxtool $< $@

clean:
	rm -f buckshot.elf buckshot.3dsx
