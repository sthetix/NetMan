rwildcard = $(foreach d, $(wildcard $1*), $(filter $(subst *, %, $2), $d) $(call rwildcard, $d/, $2))

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/base_rules

################################################################################

IPL_LOAD_ADDR := 0x40008000
MAGIC = 0x594C4648 #"HFLY"
include ./Versions.inc

################################################################################

TARGET := NetMan
BUILDDIR := build
OUTPUTDIR := output
SOURCEDIR := source
BDKDIR := bdk
BDKINC := -I./$(BDKDIR)
VPATH = $(dir ./$(SOURCEDIR)/) $(dir $(wildcard ./$(SOURCEDIR)/*/)) $(dir $(wildcard ./$(SOURCEDIR)/*/*/))
VPATH += $(dir $(wildcard ./$(BDKDIR)/)) $(dir $(wildcard ./$(BDKDIR)/*/)) $(dir $(wildcard ./$(BDKDIR)/*/*/))
VPATH += $(dir $(wildcard ./$(BDKDIR)/ianos/elfload/))

OBJS = $(addprefix $(BUILDDIR)/$(TARGET)/, \
	start.o exception_handlers.o \
	main.o heap.o \
	gfx.o tui.o config.o hid.o \
	gui.o joycon.o \
	nx_sd.o diskio.o ff.o ffunicode.o ffsystem.o \
)

OBJS += $(addprefix $(BUILDDIR)/$(TARGET)/, \
	bpmp.o ccplex.o clock.o di.o gpio.o i2c.o irq.o mc.o sdram.o \
	pinmux.o pmc.o uart.o fuse.o minerva.o hw_init.o \
	sdmmc.o sdmmc_driver.o \
	bq24193.o max17050.o max7762x.o max77620-rtc.o regulator_5v.o \
	se.o ianos.o elfload.o elfreloc_arm.o \
	btn.o touch.o ini.o sprintf.o util.o dirlist.o \
)

GFX_INC   := '"../$(SOURCEDIR)/gfx/gfx.h"'
FFCFG_INC := '"../$(SOURCEDIR)/libs/fatfs/ffconf.h"'

################################################################################

CUSTOMDEFINES := -DIPL_LOAD_ADDR=$(IPL_LOAD_ADDR) -DLP_MAGIC=$(MAGIC)
CUSTOMDEFINES += -DLP_VER_MJ=$(BLVERSION_MAJOR) -DLP_VER_MN=$(BLVERSION_MINOR) -DLP_VER_BF=$(BLVERSION_HOTFX) -DLP_RESERVED=$(BLVERSION_RSVD)
CUSTOMDEFINES += -DGFX_INC=$(GFX_INC) -DFFCFG_INC=$(FFCFG_INC)

#CUSTOMDEFINES += -DDEBUG

# UART Logging: Max baudrate 12.5M.
# DEBUG_UART_PORT - 0: UART_A, 1: UART_B, 2: UART_C.
#CUSTOMDEFINES += -DDEBUG_UART_BAUDRATE=115200 -DDEBUG_UART_INVERT=0 -DDEBUG_UART_PORT=0

#TODO: Considering reinstating some of these when pointer warnings have been fixed.
WARNINGS := -Wall -Wno-array-bounds -Wno-stringop-overread -Wno-stringop-overflow

ARCH := -march=armv4t -mtune=arm7tdmi -mthumb -mthumb-interwork
CFLAGS = $(ARCH) -Os -g -nostdlib -ffunction-sections -fdata-sections -fomit-frame-pointer -std=gnu11 $(WARNINGS) $(CUSTOMDEFINES)
LDFLAGS = $(ARCH) -nostartfiles -lgcc -Wl,--nmagic,--gc-sections -Xlinker --defsym=IPL_LOAD_ADDR=$(IPL_LOAD_ADDR)

################################################################################

.PHONY: all clean zip

all: $(OUTPUTDIR)/$(TARGET).bin zip
	@echo "--------------------------------------"
	@if [ -f $(OUTPUTDIR)/$(TARGET)_unc.bin ]; then \
		UNC_SIZE=$$(wc -c < $(OUTPUTDIR)/$(TARGET)_unc.bin); \
		echo "Uncompr size:  $$UNC_SIZE Bytes"; \
		echo "Uncompr Max:   140288 Bytes + 3 KiB BSS"; \
		if [ $$UNC_SIZE -gt 140288 ]; then echo "\e[1;33mUncompr size exceeds limit!\e[0m"; fi; \
	fi
	@BIN_SIZE=$$(wc -c < $(OUTPUTDIR)/$(TARGET).bin); \
	echo "Payload size: $$BIN_SIZE Bytes"; \
	echo "Payload Max:  131072 Bytes (128 KiB)"; \
	if [ $$BIN_SIZE -gt 131072 ]; then echo "\e[1;33mPayload size exceeds limit!\e[0m"; fi
	@echo "--------------------------------------"

zip: $(OUTPUTDIR)/$(TARGET).bin
	@mkdir -p $(OUTPUTDIR)/zip_temp/bootloader/payloads
	@mkdir -p $(OUTPUTDIR)/zip_temp/config/netman/tce/1.11.2
	@cp $(OUTPUTDIR)/$(TARGET).bin $(OUTPUTDIR)/zip_temp/bootloader/payloads/$(TARGET).bin
	@cp asset/modded_package3/1.11.2/package3 $(OUTPUTDIR)/zip_temp/config/netman/tce/1.11.2/package3
	@cp asset/tcm/emummc.bmp $(OUTPUTDIR)/zip_temp/config/netman/tce/emummc.bmp
	@cp asset/tcm/sysmmc.bmp $(OUTPUTDIR)/zip_temp/config/netman/tce/sysmmc.bmp
	@cd $(OUTPUTDIR)/zip_temp && zip -r ../$(TARGET)-$(BLVERSION_MAJOR).$(BLVERSION_MINOR).$(BLVERSION_HOTFX).zip bootloader config
	@rm -rf $(OUTPUTDIR)/zip_temp
	@echo "Created $(OUTPUTDIR)/$(TARGET)-$(BLVERSION_MAJOR).$(BLVERSION_MINOR).$(BLVERSION_HOTFX).zip"

clean:
	@rm -rf $(BUILDDIR)
	@rm -rf $(OUTPUTDIR)

$(OUTPUTDIR)/$(TARGET).bin: $(BUILDDIR)/$(TARGET)/$(TARGET).elf
	@mkdir -p "$(@D)"
	$(OBJCOPY) -S -O binary $< $@

$(BUILDDIR)/$(TARGET)/$(TARGET).elf: $(OBJS)
	@$(CC) $(LDFLAGS) -T $(SOURCEDIR)/link.ld $^ -o $@
	@echo "NetMan was built with the following flags:\nCFLAGS:  "$(CFLAGS)"\nLDFLAGS: "$(LDFLAGS)

$(BUILDDIR)/$(TARGET)/%.o: %.c
	@mkdir -p "$(@D)"
	@echo Building $@
	@$(CC) $(CFLAGS) $(BDKINC) -c $< -o $@

$(BUILDDIR)/$(TARGET)/%.o: %.S
	@mkdir -p "$(@D)"
	@echo Building $@
	@$(CC) $(CFLAGS) -c $< -o $@
