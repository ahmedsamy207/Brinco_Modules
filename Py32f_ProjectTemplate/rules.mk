# Verbosity: make V=1 for full command output
V ?= 0

ifeq ($(V),0)
Q := @
endif


###############################################################################
# OS Detection
###############################################################################

ifeq ($(OS),Windows_NT)
    HOST_OS := Windows
else
    HOST_OS := Unix
endif


###############################################################################
# Toolchain
###############################################################################

ifneq ($(ARM_TOOLCHAIN),)
    PREFIX := $(ARM_TOOLCHAIN)/arm-none-eabi-
else
    PREFIX := arm-none-eabi-
endif


CC      := $(PREFIX)gcc
XX      := $(PREFIX)g++
AS      := $(PREFIX)as
LD      := $(PREFIX)ld
OBJCOPY := $(PREFIX)objcopy
OBJDUMP := $(PREFIX)objdump



###############################################################################
# Project directories
###############################################################################

TOP  := .
TDIR := $(TOP)/$(TEMP_DIR)
BDIR := $(TOP)/$(BUILD_DIR)



###############################################################################
# Source discovery
###############################################################################

CSOURCES := $(foreach dir,$(CDIRS),$(wildcard $(dir)/*.c))
CSOURCES += $(addprefix $(TOP)/,$(CFILES))

CSOURCES := $(filter-out $(EXCLUDE_SOURCES),$(CSOURCES))


CPPSOURCES := $(foreach dir,$(CDIRS),$(wildcard $(dir)/*.cpp))
CPPSOURCES += $(addprefix $(TOP)/,$(CPPFILES))


ASOURCES := $(foreach dir,$(ADIRS),$(wildcard $(dir)/*.s))
ASOURCES += $(addprefix $(TOP)/,$(AFILES))


# Remove ./ prefix

CSOURCES   := $(patsubst ./%,%,$(CSOURCES))
CPPSOURCES := $(patsubst ./%,%,$(CPPSOURCES))
ASOURCES   := $(patsubst ./%,%,$(ASOURCES))



###############################################################################
# Object files
###############################################################################

OBJS := $(CSOURCES:%.c=$(TDIR)/%.o)
OBJS += $(CPPSOURCES:%.cpp=$(TDIR)/%.o)
OBJS += $(ASOURCES:%.s=$(TDIR)/%.o)



###############################################################################
# Compiler flags
###############################################################################

ARCH_FLAGS := -mcpu=cortex-m0plus -mthumb

DEBUG_FLAGS ?= -gdwarf-3
OPT ?= -Os


TGT_CFLAGS ?= \
$(ARCH_FLAGS) \
$(DEBUG_FLAGS) \
$(OPT) \
-std=c17 \
-nostdlib \
-ffreestanding \
-fno-builtin \
-Wall \
-ffunction-sections \
-fdata-sections \
$(addprefix -D,$(LIB_FLAGS))


TGT_CPPFLAGS ?= \
$(ARCH_FLAGS) \
$(DEBUG_FLAGS) \
$(OPT) \
-std=c++11 \
-Wall \
-ffunction-sections \
-fdata-sections \
-fno-exceptions \
-fno-rtti \
$(addprefix -D,$(LIB_FLAGS))


TGT_ASFLAGS ?= \
$(ARCH_FLAGS) \
$(DEBUG_FLAGS) \
$(OPT) \
-Wa,--warn



TGT_LDFLAGS ?= \
$(ARCH_FLAGS) \
-specs=nano.specs \
-lstdc++ \
-Wl,-Map=$(BDIR)/$(PROJECT).map \
-Wl,--gc-sections \
-Wl,--no-warn-rwx-segments \
-Wl,--print-memory-usage


ifdef ENABLE_PRINTF_FLOAT
TGT_LDFLAGS += -u _printf_float
endif



TGT_INCFLAGS := $(addprefix -I$(TOP)/,$(INCLUDES))



###############################################################################
# Helpers
###############################################################################

# mkdir is handled by the compiler rules
# Use portable shell commands

MKDIR = mkdir -p "$1"


ifeq ($(HOST_OS),Windows)

RMDIR = rm -rf "$1"

else

RMDIR = rm -rf "$1"

endif



###############################################################################
# Targets
###############################################################################

.PHONY: all clean flash echo


all: $(BDIR)/$(PROJECT).elf \
     $(BDIR)/$(PROJECT).bin \
     $(BDIR)/$(PROJECT).hex \
     $(BDIR)/$(PROJECT).lst



echo:
	$(info --- Source Files ---)
	$(info C: $(CSOURCES))
	$(info CPP: $(CPPSOURCES))
	$(info ASM: $(ASOURCES))
	$(info OBJS: $(OBJS))
	$(info INCLUDE FLAGS: $(TGT_INCFLAGS))



###############################################################################
# Compile C
###############################################################################

$(TDIR)/%.o: %.c

	@echo [CC] $<
	$(Q)$(call MKDIR,$(dir $@))
	$(Q)$(CC) $(TGT_CFLAGS) $(TGT_INCFLAGS) \
	-MT $@ \
	-o $@ \
	-c $< \
	-MD -MF $(TDIR)/$*.d -MP



###############################################################################
# Compile C++
###############################################################################

$(TDIR)/%.o: %.cpp

	@echo [C++] $<
	$(Q)$(call MKDIR,$(dir $@))
	$(Q)$(XX) $(TGT_CPPFLAGS) $(TGT_INCFLAGS) \
	-MT $@ \
	-o $@ \
	-c $< \
	-MD -MF $(TDIR)/$*.d -MP



###############################################################################
# Compile ASM
###############################################################################

$(TDIR)/%.o: %.s

	@echo [AS] $<
	$(Q)$(call MKDIR,$(dir $@))
	$(Q)$(CC) $(TGT_ASFLAGS) -o $@ -c $<



###############################################################################
# Link
###############################################################################

$(BDIR)/$(PROJECT).elf: $(OBJS)

	@echo [LD] Linking $(PROJECT).elf
	$(Q)$(call MKDIR,$(dir $@))
	$(Q)$(XX) \
	$(TGT_LDFLAGS) \
	-T$(TOP)/$(LDSCRIPT) \
	$(OBJS) \
	-o $@



###############################################################################
# Binary files
###############################################################################

$(BDIR)/%.bin: $(BDIR)/%.elf

	@echo [BIN] $@
	$(Q)$(OBJCOPY) \
	-I elf32-littlearm \
	-O binary \
	$< $@



$(BDIR)/%.hex: $(BDIR)/%.elf

	@echo [HEX] $@
	$(Q)$(OBJCOPY) \
	-I elf32-littlearm \
	-O ihex \
	$< $@



$(BDIR)/%.lst: $(BDIR)/%.elf

	@echo [LST] $@
	$(Q)$(OBJDUMP) --source $< > $@



###############################################################################
# Clean
###############################################################################

clean:

	$(Q)$(call RMDIR,$(TDIR))
	$(Q)$(call RMDIR,$(BDIR))



###############################################################################
# Flash
###############################################################################

flash: $(BDIR)/$(PROJECT).elf


ifeq ($(FLASH_PROGRM),jlink)

	$(JLINKEXE) \
	-device $(JLINK_DEVICE) \
	-if swd \
	-speed 4000 \
	-JLinkScriptFile $(TOP)/Misc/jlink-script \
	-CommanderScript $(TOP)/Misc/jlink-command


else ifeq ($(FLASH_PROGRM),pyocd)

	$(PYOCD_EXE) erase \
	-t $(PYOCD_DEVICE) \
	--chip \
	--config $(TOP)/Misc/pyocd.yaml


	$(PYOCD_EXE) load $< \
	-t $(PYOCD_DEVICE) \
	--config $(TOP)/Misc/pyocd.yaml


else

	@echo FLASH_PROGRM is invalid

endif
