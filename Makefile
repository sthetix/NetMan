ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/base_rules

################################################################################
# Configuration
################################################################################

IPL_LOAD_ADDR := 0x40008000
IPL_MAGIC := 0x594C4648 #"HFLY"
include ./Versions.inc

################################################################################
# Directories and Output
################################################################################

TARGET := NetMan
BUILDDIR := build
OUTPUTDIR := output
DISTDIR := dist
SOURCEDIR := netman
BDKDIR := bdk
BDKINC := -I./$(BDKDIR)
VPATH = $(dir ./$(SOURCEDIR)/) $(dir $(wildcard ./$(SOURCEDIR)/*/)) $(dir $(wildcard ./$(SOURCEDIR)/*/*/))
VPATH += $(dir $(wildcard ./$(BDKDIR)/)) $(dir $(wildcard ./$(BDKDIR)/*/)) $(dir $(wildcard ./$(BDKDIR)/*/*/))
VPATH += $(dir $(wildcard ./$(BDKDIR)/ianos/elfload/))

# Main and graphics.
OBJS = $(addprefix $(BUILDDIR)/$(TARGET)/, \
    start.o exception_handlers.o \
    main.o heap.o \
    gfx.o tui.o screenshot.o config.o \
)

# Hardware.
OBJS += $(addprefix $(BUILDDIR)/$(TARGET)/, \
    bpmp.o ccplex.o clock.o di.o gpio.o i2c.o irq.o mc.o sdram.o \
    pinmux.o pmc.o uart.o \
    fuse.o minerva.o \
    sdmmc.o sdmmc_driver.o emmc.o sd.o \
    bq24193.o max17050.o max7762x.o max77620-rtc.o regulator_5v.o \
    hw_init.o \
    se.o ianos.o elfload.o elfreloc_arm.o \
)

# Utilities.
OBJS += $(addprefix $(BUILDDIR)/$(TARGET)/, \
    btn.o touch.o ini.o sprintf.o util.o \
)

# Libraries.
OBJS += $(addprefix $(BUILDDIR)/$(TARGET)/, \
    diskio.o ff.o ffunicode.o ffsystem.o \
)

# LVGL library for UI.
OBJS += $(addprefix $(BUILDDIR)/$(TARGET)/, \
    $(patsubst $(BDKDIR)/libs/lvgl/%.c, %.o, $(wildcard $(BDKDIR)/libs/lvgl/lv_core/*.c)) \
    $(patsubst $(BDKDIR)/libs/lvgl/%.c, %.o, $(wildcard $(BDKDIR)/libs/lvgl/lv_draw/*.c)) \
    $(patsubst $(BDKDIR)/libs/lvgl/%.c, %.o, $(wildcard $(BDKDIR)/libs/lvgl/lv_fonts/*.c)) \
    $(patsubst $(BDKDIR)/libs/lvgl/%.c, %.o, $(wildcard $(BDKDIR)/libs/lvgl/lv_hal/*.c)) \
    $(patsubst $(BDKDIR)/libs/lvgl/%.c, %.o, $(wildcard $(BDKDIR)/libs/lvgl/lv_misc/*.c)) \
    $(patsubst $(BDKDIR)/libs/lvgl/%.c, %.o, $(wildcard $(BDKDIR)/libs/lvgl/lv_objx/*.c)) \
    $(patsubst $(BDKDIR)/libs/lvgl/%.c, %.o, $(wildcard $(BDKDIR)/libs/lvgl/lv_themes/*.c)) \
)

GFX_INC   := '"../$(SOURCEDIR)/gfx/gfx.h"'
FFCFG_INC := '"../$(SOURCEDIR)/libs/fatfs/ffconf.h"'

################################################################################
# Build Flags
################################################################################

CUSTOMDEFINES := -DIPL_LOAD_ADDR=$(IPL_LOAD_ADDR) -DBL_MAGIC=$(IPL_MAGIC)
CUSTOMDEFINES += -DBL_VER_MJ=$(BLVERSION_MAJOR) -DBL_VER_MN=$(BLVERSION_MINOR) -DBL_VER_HF=$(BLVERSION_HOTFX) -DBL_RESERVED=$(BLVERSION_RSVD)
CUSTOMDEFINES += -DNYX_VER_MJ=$(NYXVERSION_MAJOR) -DNYX_VER_MN=$(NYXVERSION_MINOR) -DNYX_VER_HF=$(NYXVERSION_HOTFX) -DNYX_RESERVED=$(NYXVERSION_RSVD)

# BDK defines.
CUSTOMDEFINES += -DBDK_EMUMMC_ENABLE
CUSTOMDEFINES += -DGFX_INC=$(GFX_INC) -DFFCFG_INC=$(FFCFG_INC)

# Uncomment for UART debugging if needed.
#CUSTOMDEFINES += -DDEBUG_UART_BAUDRATE=115200 -DDEBUG_UART_INVERT=0 -DDEBUG_UART_PORT=0

WARNINGS := -Wall -Wno-array-bounds -Wno-stringop-overread -Wno-stringop-overflow

ARCH := -march=armv4t -mtune=arm7tdmi -mthumb -mthumb-interwork
CFLAGS = $(ARCH) -O2 -g -nostdlib -ffunction-sections -fdata-sections -fomit-frame-pointer -fno-inline -std=gnu11 $(WARNINGS) $(CUSTOMDEFINES)
LDFLAGS = $(ARCH) -nostartfiles -lgcc -Wl,--nmagic,--gc-sections -Xlinker --defsym=IPL_LOAD_ADDR=$(IPL_LOAD_ADDR)

################################################################################
# Build Rules
################################################################################

.PHONY: all clean dist zip help

.DEFAULT_GOAL := all

all: $(TARGET).bin
	@echo "--------------------------------------"
	@echo -n "Payload size: "
	$(eval BIN_SIZE = $(shell wc -c < $(OUTPUTDIR)/$(TARGET).bin))
	@echo $(BIN_SIZE)" Bytes"
	@echo "Payload Max:  126296 Bytes"
	@if [ ${BIN_SIZE} -gt 126296 ]; then echo "\e[1;33mPayload size exceeds limit!\e[0m"; fi
	@echo "--------------------------------------"

clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILDDIR)
	@rm -rf $(OUTPUTDIR)
	@rm -rf $(DISTDIR)
	@echo "Clean complete."

dist: $(TARGET).bin
	@echo "Creating distribution package..."
	@mkdir -p $(DISTDIR)/bootloader/payloads
	@cp $(OUTPUTDIR)/$(TARGET).bin $(DISTDIR)/bootloader/payloads/
	@echo "Distribution structure created in $(DISTDIR)/"

zip: dist
	@echo "Creating release archive..."
	@cd $(DISTDIR) && zip -r ../$(TARGET).zip bootloader/
	@echo "--------------------------------------"
	@echo "Release package created: $(TARGET).zip"
	@echo "Contents: bootloader/payloads/$(TARGET).bin"
	@echo "--------------------------------------"

help:
	@echo "NetMan Build System"
	@echo "==================="
	@echo ""
	@echo "Available targets:"
	@echo "  all     - Build the payload binary (default)"
	@echo "  clean   - Remove all build artifacts"
	@echo "  dist    - Create distribution directory structure"
	@echo "  zip     - Create release zip file with proper structure"
	@echo "  help    - Display this help message"
	@echo ""
	@echo "Output:"
	@echo "  Binary:  $(OUTPUTDIR)/$(TARGET).bin"
	@echo "  Archive: $(TARGET).zip"
	@echo ""

$(TARGET).bin: $(BUILDDIR)/$(TARGET)/$(TARGET).elf
	@mkdir -p $(OUTPUTDIR)
	$(OBJCOPY) -S -O binary $< $(OUTPUTDIR)/$@

$(BUILDDIR)/$(TARGET)/$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) -T $(SOURCEDIR)/link.ld $^ -o $@
	@echo "NetMan was built with the following flags:\nCFLAGS:  "$(CFLAGS)"\nLDFLAGS: "$(LDFLAGS)

$(shell mkdir -p build/NetMan/lv_core build/NetMan/lv_draw build/NetMan/lv_fonts build/NetMan/lv_hal build/NetMan/lv_misc build/NetMan/lv_objx build/NetMan/lv_themes build/NetMan/ianos/elfload)

$(BUILDDIR)/$(TARGET)/%.o: %.c
	@echo Building $@
	$(CC) $(CFLAGS) $(BDKINC) -c $< -o $@

$(BUILDDIR)/$(TARGET)/%.o: %.S
	@echo Building $@
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJS): $(BUILDDIR)/$(TARGET)

$(BUILDDIR)/$(TARGET):
	@mkdir -p "$(BUILDDIR)"
	@mkdir -p "$(BUILDDIR)/$(TARGET)"
	@mkdir -p "$(OUTPUTDIR)"