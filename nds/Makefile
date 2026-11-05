# OpenPak DS Patcher (.nds). Build in devkitpro/devkitarm: make APP_VERSION=x.y.z
ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment")
endif
include $(DEVKITARM)/ds_rules

TARGET   := $(notdir $(CURDIR))
BUILD    := build
SOURCES  := source
APP_VERSION ?= 0.0.0
GAME_TITLE := OpenPak DS Patcher
GAME_SUBTITLE1 := Switch the DS to OpenPak
GAME_SUBTITLE2 := openpak.org
GAME_ICON := $(CURDIR)/../icon.bmp
ARCH     := -mthumb -mthumb-interwork
CFLAGS   := -g -Wall -O2 -march=armv5te -mtune=arm946e-s -fomit-frame-pointer -ffast-math $(ARCH) \
            -DARM9 -DAPP_VERSION=\"$(APP_VERSION)\" -I$(LIBNDS)/include
CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17
LDFLAGS  = -specs=ds_arm9.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LIBS     := -lfat -lnds9
LIBDIRS  := $(LIBNDS)

ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(CURDIR)/OpenPak-DS-Patcher
export VPATH := $(CURDIR)/$(SOURCES)
export DEPSDIR := $(CURDIR)/$(BUILD)
export OFILES := $(patsubst %.cpp,%.o,$(notdir $(wildcard $(SOURCES)/*.cpp)))
export LD := $(CXX)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export APP_VERSION
.PHONY: $(BUILD) clean
$(BUILD):
	@mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
clean:
	rm -rf $(BUILD) OpenPak-DS-Patcher.elf OpenPak-DS-Patcher.nds
else
$(OUTPUT).nds: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)
-include $(DEPSDIR)/*.d
endif
