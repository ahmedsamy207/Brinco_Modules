# Verbosity: make V=1 for full command output
V ?= 0
ifeq ($(V),0)
Q := @
endif

# The recipes below use cmd.exe syntax ("if not exist ... mkdir ...").
# Pin the shell on Windows so an sh.exe from Git Bash / MSYS2 in PATH does
# not break the build.
ifeq ($(OS),Windows_NT)
SHELL := cmd.exe
.SHELLFLAGS := /C
endif

# Toolchain settings
ifneq ($(ARM_TOOLCHAIN),)
	PREFIX      := $(ARM_TOOLCHAIN)/arm-none-eabi-
else
	PREFIX      := arm-none-eabi-
endif
CC          = $(PREFIX)gcc
XX          = $(PREFIX)g++
AS          = $(PREFIX)as
LD          = $(PREFIX)ld
OBJCOPY     = $(PREFIX)objcopy
OBJDUMP     = $(PREFIX)objdump

# Project config
TOP         := .
TDIR        := $(TOP)/$(TEMP_DIR)
BDIR        := $(TOP)/$(BUILD_DIR)

# CMSIS device family pack used by pyOCD to resolve the PY32 target.
# It ships with the project, so no pack download/installation is required.
PYOCD_PACK  ?= $(TOP)/Tools/packs/Puya.PY32F0xx_DFP.1.2.0.pack

# Source discovery
CSOURCES    := $(foreach dir, $(CDIRS), $(wildcard $(dir)/*.c))
CSOURCES    += $(addprefix $(TOP)/, $(CFILES))

CPPSOURCES  := $(foreach dir, $(CDIRS), $(wildcard $(dir)/*.cpp))
CPPSOURCES  += $(addprefix $(TOP)/, $(CPPFILES))

ASOURCES    := $(foreach dir, $(ADIRS), $(wildcard $(dir)/*.s))
ASOURCES    += $(addprefix $(TOP)/, $(AFILES))

# Normalize paths: remove leading './'
CSOURCES    := $(patsubst ./%,%,$(CSOURCES))
CPPSOURCES  := $(patsubst ./%,%,$(CPPSOURCES))
ASOURCES    := $(patsubst ./%,%,$(ASOURCES))

# Object and dependency files
OBJS        := $(CSOURCES:%.c=$(TDIR)/%.o)
OBJS        += $(CPPSOURCES:%.cpp=$(TDIR)/%.o)
OBJS        += $(ASOURCES:%.s=$(TDIR)/%.o)

# Compiler flags
ARCH_FLAGS      := -mcpu=cortex-m0plus -mthumb
DEBUG_FLAGS     ?= -gdwarf-3
OPT             ?= -Os

TGT_CFLAGS      ?= $(ARCH_FLAGS) $(DEBUG_FLAGS) $(OPT) -std=c17 -nostdlib -ffreestanding -fno-builtin -Wall -ffunction-sections -fdata-sections $(addprefix -D, $(LIB_FLAGS))
TGT_CPPFLAGS    ?= $(ARCH_FLAGS) $(DEBUG_FLAGS) $(OPT) -std=c++11 -Wall -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti $(addprefix -D, $(LIB_FLAGS))
TGT_ASFLAGS     ?= $(ARCH_FLAGS) $(DEBUG_FLAGS) $(OPT) -Wa,--warn

TGT_LDFLAGS     ?= $(ARCH_FLAGS) \
	-specs=nano.specs \
	-lstdc++ \
    -Wl,-Map=$(BDIR)/$(PROJECT).map \
    -Wl,--gc-sections \
    -Wl,--print-memory-usage

ifdef ENABLE_PRINTF_FLOAT
TGT_LDFLAGS += -u _printf_float
endif

TGT_INCFLAGS := $(addprefix -I$(TOP)/, $(INCLUDES))

# Targets
.PHONY: all clean flash erase echo

all: $(BDIR)/$(PROJECT).elf $(BDIR)/$(PROJECT).bin $(BDIR)/$(PROJECT).hex $(BDIR)/$(PROJECT).lst

echo:
	$(info --- Source Files ---)
	$(info C: $(CSOURCES))
	$(info CPP: $(CPPSOURCES))
	$(info ASM: $(ASOURCES))
	$(info OBJS: $(OBJS))
	$(info INCLUDE FLAGS: $(TGT_INCFLAGS))

# Compile C source to object
$(TDIR)/%.o: %.c
	@echo [CC] $<
	@if not exist "$(dir $@)" mkdir "$(dir $@)"
	$(Q)$(CC) $(TGT_CFLAGS) $(TGT_INCFLAGS) -MT $@ -o $@ -c $< -MD -MF $(TDIR)/$*.d -MP

# Compile C++ source to object
$(TDIR)/%.o: %.cpp
	@echo [C++] $<
	@if not exist "$(dir $@)" mkdir "$(dir $@)"
	$(Q)$(XX) $(TGT_CPPFLAGS) $(TGT_INCFLAGS) -MT $@ -o $@ -c $< -MD -MF $(TDIR)/$*.d -MP

# Compile ASM source to object
$(TDIR)/%.o: %.s
	@echo [AS] $<
	@if not exist "$(dir $@)" mkdir "$(dir $@)"
	$(Q)$(CC) $(TGT_ASFLAGS) -o $@ -c $<

# Link ELF
$(BDIR)/$(PROJECT).elf: $(OBJS)
	@echo [LD] Linking $(PROJECT).elf
	@if not exist "$(dir $@)" mkdir "$(dir $@)"
	$(Q)$(XX) $(TGT_LDFLAGS) -T$(TOP)/$(LDSCRIPT) $(OBJS) -o $@

# Generate BIN from ELF
$(BDIR)/%.bin: $(BDIR)/%.elf
	@echo [BIN] $@
	$(Q)$(OBJCOPY) -I elf32-littlearm -O binary $< $@

# Generate HEX from ELF
$(BDIR)/%.hex: $(BDIR)/%.elf
	@echo [HEX] $@
	$(Q)$(OBJCOPY) -I elf32-littlearm -O ihex $< $@

# Generate LST from ELF
$(BDIR)/%.lst: $(BDIR)/%.elf
	@echo [LST] $@
	$(Q)$(OBJDUMP) --source $< > $@

# Clean build artifacts
clean:
	@if exist "$(TDIR)" rmdir /S /Q "$(TDIR)"
	@if exist "$(BDIR)" rmdir /S /Q "$(BDIR)"

# Erase the whole chip (pyOCD only)
erase:
ifeq ($(FLASH_PROGRM),pyocd)
	"$(PYOCD_EXE)" erase -t $(PYOCD_DEVICE) --pack "$(PYOCD_PACK)" --chip --config "$(TOP)/pyocd.yaml"
else
	@echo erase is only implemented for FLASH_PROGRM=pyocd
endif

# Flash (optional)
flash: $(BDIR)/$(PROJECT).elf
ifeq ($(FLASH_PROGRM),jlink)
	$(JLINKEXE) -device $(JLINK_DEVICE) -if swd -speed 4000 -JLinkScriptFile $(TOP)/Misc/jlink-script -CommanderScript $(TOP)/Misc/jlink-command
else ifeq ($(FLASH_PROGRM),pyocd)
	"$(PYOCD_EXE)" erase -t $(PYOCD_DEVICE) --pack "$(PYOCD_PACK)" --chip --config "$(TOP)/pyocd.yaml"
	"$(PYOCD_EXE)" load $< -t $(PYOCD_DEVICE) --pack "$(PYOCD_PACK)" --config "$(TOP)/pyocd.yaml"
else
	@echo FLASH_PROGRM is invalid
endif
