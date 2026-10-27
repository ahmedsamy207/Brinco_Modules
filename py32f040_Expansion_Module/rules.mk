# Verbosity: make V=1 for full command output
V ?= 0

ifeq ($(V),0)
Q := @
endif

# The recipes below use cmd.exe syntax ("if not exist ... mkdir ..." and
# "rmdir /S /Q"). Pin the shell on Windows so an sh.exe from Git Bash /
# MSYS2 in PATH cannot break the build.
ifeq ($(OS),Windows_NT)
    SHELL := cmd.exe
    .SHELLFLAGS := /C
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

# The executable is quoted as a whole, so a toolchain installed under
# "C:\Program Files (x86)\..." (spaces are the default on Windows) works too.
ifneq ($(ARM_TOOLCHAIN),)
    ARM_BIN := $(ARM_TOOLCHAIN)/
else
    ARM_BIN :=
endif


CC      := "$(ARM_BIN)arm-none-eabi-gcc"
XX      := "$(ARM_BIN)arm-none-eabi-g++"
AS      := "$(ARM_BIN)arm-none-eabi-as"
LD      := "$(ARM_BIN)arm-none-eabi-ld"
OBJCOPY := "$(ARM_BIN)arm-none-eabi-objcopy"
OBJDUMP := "$(ARM_BIN)arm-none-eabi-objdump"



###############################################################################
# Project directories
###############################################################################

TOP  := .
TDIR := $(TOP)/$(TEMP_DIR)
BDIR := $(TOP)/$(BUILD_DIR)


###############################################################################
# OpenOCD (the bundled Puya build) - the ST-Link V2 / V2-1 programming path
###############################################################################

OPENOCD_EXE     ?= $(TOP)/Tools/openocd-puya/bin/openocd.exe
OPENOCD_SCRIPTS ?= $(TOP)/Tools/openocd-puya/script
OPENOCD_DIR     ?= $(TOP)/Tools/openocd-puya
OPENOCD_CFG     ?= $(OPENOCD_DIR)/stlink-py32f040.cfg
# Flash bank size used by `erase-openocd` (128 KB on PY32F040xB)
OPENOCD_FLASH_SIZE ?= 0x20000
# Drop "verify" from here if the flash driver cannot read back:
#   make flash-openocd OPENOCD_PROGRAM_ARGS="reset exit"
OPENOCD_PROGRAM_ARGS ?= verify reset exit

OPENOCD_CMD = "$(OPENOCD_EXE)" -s "$(OPENOCD_SCRIPTS)" -s "$(OPENOCD_DIR)" -f "$(OPENOCD_CFG)"



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

# Directory helpers.
# On Windows the recipes run through cmd.exe: "mkdir -p" would create a folder
# literally named "-p" and "rm" does not exist at all.

ifeq ($(HOST_OS),Windows)

MKDIR = if not exist "$(subst /,\,$1)" mkdir "$(subst /,\,$1)"
RMDIR = if exist "$(subst /,\,$1)" rmdir /S /Q "$(subst /,\,$1)"

else

MKDIR = mkdir -p "$1"
RMDIR = rm -rf "$1"

endif



###############################################################################
# Targets
###############################################################################

.PHONY: all clean flash erase echo flash-openocd erase-openocd


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
	-MT "$@" \
	-o "$@" \
	-c "$<" \
	-MD -MF "$(TDIR)/$*.d" -MP



###############################################################################
# Compile C++
###############################################################################

$(TDIR)/%.o: %.cpp

	@echo [C++] $<
	$(Q)$(call MKDIR,$(dir $@))
	$(Q)$(XX) $(TGT_CPPFLAGS) $(TGT_INCFLAGS) \
	-MT "$@" \
	-o "$@" \
	-c "$<" \
	-MD -MF "$(TDIR)/$*.d" -MP



###############################################################################
# Compile ASM
###############################################################################

$(TDIR)/%.o: %.s

	@echo [AS] $<
	$(Q)$(call MKDIR,$(dir $@))
	$(Q)$(CC) $(TGT_ASFLAGS) -o "$@" -c "$<"



###############################################################################
# Link
###############################################################################

$(BDIR)/$(PROJECT).elf: $(OBJS)

	@echo [LD] Linking $(PROJECT).elf
	$(Q)$(call MKDIR,$(dir $@))
	$(Q)$(XX) \
	$(TGT_LDFLAGS) \
	-T"$(TOP)/$(LDSCRIPT)" \
	$(OBJS) \
	-o "$@"



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

erase:

ifeq ($(FLASH_PROGRM),pyocd)

	$(PYOCD_EXE) erase \
	-t $(PYOCD_DEVICE) \
	--pack "$(PYOCD_PACK)" \
	--chip \
	--config "$(TOP)/pyocd.yaml"

else

	@echo erase is only implemented for FLASH_PROGRM=pyocd

endif


flash-openocd: $(BDIR)/$(PROJECT).hex
	$(OPENOCD_CMD) -c "program $(BDIR)/$(PROJECT).hex $(OPENOCD_PROGRAM_ARGS)"


erase-openocd:
	$(OPENOCD_CMD) -c "init; reset halt; flash erase_address 0x08000000 $(OPENOCD_FLASH_SIZE); shutdown"


# What `make flash` needs: the hex for OpenOCD (pure flash content, no RAM
# sections), the ELF for pyOCD and J-Link.
ifeq ($(FLASH_PROGRM),openocd)
FLASH_IMAGE := $(BDIR)/$(PROJECT).hex
else
FLASH_IMAGE := $(BDIR)/$(PROJECT).elf
endif


flash: $(FLASH_IMAGE)


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
	--pack "$(PYOCD_PACK)" \
	--chip \
	--config "$(TOP)/pyocd.yaml"


	$(PYOCD_EXE) load "$<" \
	-t $(PYOCD_DEVICE) \
	--pack "$(PYOCD_PACK)" \
	--config "$(TOP)/pyocd.yaml"


else ifeq ($(FLASH_PROGRM),openocd)

	$(OPENOCD_CMD) -c "program $(BDIR)/$(PROJECT).hex $(OPENOCD_PROGRAM_ARGS)"

else

	@echo FLASH_PROGRM is invalid

endif



###############################################################################
# Header dependencies (.d files generated by -MD)
###############################################################################

-include $(OBJS:.o=.d)
