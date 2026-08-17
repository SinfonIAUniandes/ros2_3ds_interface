ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

TARGET := ros2_3ds_interface
BUILD := build
SOURCES := source source/logging source/ui generated/ros_graph generated/ros_types generated/ros_imu generated/ros_services
INCLUDES := include include/logging include/ui generated/ros_graph generated/ros_types generated/ros_imu generated/ros_services
ROMFS := romfs
CYCLONEDDS_SOURCE ?= $(abspath ../cyclonedds_3ds)
CYCLONEDDS_BUILD ?= $(CYCLONEDDS_SOURCE)/build-3ds
export CYCLONEDDS_SOURCE CYCLONEDDS_BUILD

APP_TITLE := ROS 2 3DS Interface
APP_DESCRIPTION := Native DDS feasibility probe
APP_AUTHOR := Open-source contributors

ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS := -g -Wall -Wextra -Werror -O2 -mword-relocations \
	-ffunction-sections $(ARCH) $(INCLUDE) -D__3DS__
ASFLAGS := -g $(ARCH)
LDFLAGS := -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LIBS := -lddsc -lcitro2d -lcitro3d -lctru -lm
LIBDIRS := $(CTRULIB) $(CYCLONEDDS_BUILD)

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT := $(CURDIR)/$(TARGET)
export TOPDIR := $(CURDIR)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
SFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
export OFILES := $(CFILES:.c=.o) $(SFILES:.s=.o)
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
	$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
	-I$(CYCLONEDDS_SOURCE)/src/core/ddsc/include \
	-I$(CYCLONEDDS_SOURCE)/src/ddsrt/include \
	-I$(CYCLONEDDS_BUILD)/src/ddsrt/include \
	-I$(CYCLONEDDS_BUILD)/src/ddsrt/ddsrt-internal/include \
	-I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export LD := $(CC)
export _3DSXDEPS := $(OUTPUT).smdh
export _3DSXFLAGS += --smdh=$(OUTPUT).smdh --romfs=$(CURDIR)/$(ROMFS)

.PHONY: all clean

all: $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(BUILD):
	@mkdir -p $@

clean:
	@echo clean ...
	@rm -rf $(BUILD) $(TARGET).3dsx $(TARGET).elf $(TARGET).lst $(TARGET).smdh

else

$(OUTPUT).3dsx: $(OUTPUT).elf $(_3DSXDEPS)
$(OUTPUT).elf: $(OFILES)

-include $(DEPSDIR)/*.d

endif
