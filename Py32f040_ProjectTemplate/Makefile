##### Project ###############################################################

# Defaults to the folder name, so a copy of this template builds
# Build\<FolderName>.elf - which is what .vscode/launch.json expects.
PROJECT ?= $(notdir $(CURDIR))

# Generated files
TEMP_DIR  := Temp
BUILD_DIR := Build


##### MCU ###################################################################

# MCU types:
# PY32F002Ax5
# PY32F002Bx5
# PY32F003x4, PY32F003x6, PY32F003x8
# PY32F030x6, PY32F030x8
# PY32F040x6, PY32F040x8, PY32F040xB  (PY32F040C1BT6 uses PY32F040xB)
# PY32F072xB

MCU_TYPE := PY32F040xB


##### Options ###############################################################

ENABLE_VECT_TAB_MIGRATION ?= n

USE_LL_LIB  ?= n
USE_FREERTOS ?= n
USE_DSP     ?= n
USE_EPAPER  ?= n

# Programmer: pyocd or jlink
FLASH_PROGRM ?= pyocd


##### Toolchains ############################################################

ARM_TOOLCHAIN ?=

JLINKEXE ?=

# pyOCD resolution order:
#   1. PYOCD_EXE from the command line or the environment
#   2. the project virtual environment created by Tools/setup-pyocd.ps1
#   3. the Python launcher ("py -m pyocd"), which also works when the per-user
#      Scripts folder (e.g. %APPDATA%\Python\Python314\Scripts) is not in PATH
# Put quotes inside the value if the path contains spaces, e.g.
#   make flash PYOCD_EXE="C:\Users\Ahmed Samy\AppData\Roaming\Python\Python314\Scripts\pyocd.exe"
PYOCD_VENV ?= $(wildcard .venv/Scripts/pyocd.exe)
PYOCD_EXE ?= $(if $(PYOCD_VENV),"$(PYOCD_VENV)",py -m pyocd)

# CMSIS device family pack that teaches pyOCD about the PY32F0 parts.
PYOCD_PACK ?= $(TOP)/Tools/packs/Puya.PY32F0xx_DFP.1.2.0.pack


# OS detection
ifeq ($(OS),Windows_NT)
    HOST_OS := Windows
else
    HOST_OS := Unix
endif


# pyOCD / CMSIS-Pack target for the real part.
# PY32F040C1BT6 is the 48-pin (C) package with 128 KB flash / 16 KB SRAM,
# so py32f040cxb is the exact target. It has the same memory map and flash
# algorithm as PY32F040xB, which MCU_TYPE selects for the build.
PYOCD_TARGET ?= PY32F040CxB


# Convert MCU name
# NOTE: 'tr' does not exist in cmd.exe/PowerShell, so Windows uses PowerShell.
ifeq ($(HOST_OS),Windows)

    MCU_TYPE_UPPER := $(shell powershell -NoProfile -Command "'$(MCU_TYPE)'.ToUpper()")
    MCU_TYPE_LOWER := $(shell powershell -NoProfile -Command "'$(MCU_TYPE)'.ToLower()")
    PYOCD_TARGET_LOWER := $(shell powershell -NoProfile -Command "'$(PYOCD_TARGET)'.ToLower()")

else

    MCU_TYPE_UPPER := $(shell echo $(MCU_TYPE) | tr '[:lower:]' '[:upper:]')
    MCU_TYPE_LOWER := $(shell echo $(MCU_TYPE) | tr '[:upper:]' '[:lower:]')
    PYOCD_TARGET_LOWER := $(shell echo $(PYOCD_TARGET) | tr '[:upper:]' '[:lower:]')

endif

JLINK_DEVICE ?= $(MCU_TYPE_UPPER)
PYOCD_DEVICE ?= $(PYOCD_TARGET_LOWER)



##### Source paths ##########################################################

CDIRS := User/Src
CFILES :=

CPPFILES :=

ADIRS :=
AFILES :=


# Include paths

INCLUDES := \
    Libraries/CMSIS/Core/Include \
    Libraries/CMSIS/Device/PY32F0xx/Include \
    User/Inc



##### Library Flags #########################################################

LIB_FLAGS := $(MCU_TYPE)


ifeq ($(ENABLE_VECT_TAB_MIGRATION),n)
    LIB_FLAGS += FORBID_VECT_TAB_MIGRATION
endif



##### Linker script #########################################################

LDSCRIPT = Libraries/LDScripts/$(MCU_TYPE_LOWER).ld



#############################################################################
# PY32F002B
#############################################################################

ifneq (,$(findstring PY32F002B,$(MCU_TYPE)))

CFILES += Libraries/CMSIS/Device/PY32F0xx/Source/system_py32f002b.c


ifeq ($(USE_LL_LIB),y)

CDIRS += \
    Libraries/PY32F002B_LL_Driver/Src \
    Libraries/PY32F002B_LL_BSP/Src

INCLUDES += \
    Libraries/PY32F002B_LL_Driver/Inc \
    Libraries/PY32F002B_LL_BSP/Inc

LIB_FLAGS += USE_FULL_LL_DRIVER


else

CDIRS += \
    Libraries/PY32F002B_HAL_Driver/Src \
    Libraries/PY32F002B_HAL_BSP/Src

INCLUDES += \
    Libraries/PY32F002B_HAL_Driver/Inc \
    Libraries/PY32F002B_HAL_BSP/Inc

endif


EXCLUDE_SOURCES :=


AFILES := \
    Libraries/CMSIS/Device/PY32F0xx/Source/gcc/startup_py32f002b.s



#############################################################################
# PY32F07x
#############################################################################

else ifneq (,$(findstring PY32F07,$(MCU_TYPE)))


CFILES += Libraries/CMSIS/Device/PY32F0xx/Source/system_py32f07x.c


CDIRS += \
    Libraries/PY32F07x_HAL_Driver/Src \
    Libraries/PY32F07x_HAL_BSP/Src


INCLUDES += \
    Libraries/PY32F07x_HAL_Driver/Inc \
    Libraries/PY32F07x_HAL_BSP/Inc


EXCLUDE_SOURCES :=


LIB_FLAGS += USE_HAL_DRIVER


AFILES := \
    Libraries/CMSIS/Device/PY32F0xx/Source/gcc/startup_py32f072.s



#############################################################################
# PY32F040
#############################################################################

else ifneq (,$(findstring PY32F040,$(MCU_TYPE)))


CFILES += Libraries/CMSIS/Device/PY32F0xx/Source/system_py32f040.c


CDIRS += \
    Libraries/PY32F040_HAL_Driver/Src


INCLUDES += \
    Libraries/PY32F040_HAL_Driver/Inc


EXCLUDE_SOURCES := \
    %py32f040_hal_timebase_tim_template.c \
    %py32f040_hal_timebase_rtc_alarm_template.c


LIB_FLAGS += USE_HAL_DRIVER


AFILES := \
    Libraries/CMSIS/Device/PY32F0xx/Source/gcc/startup_py32f040.s



#############################################################################
# PY32F002A / PY32F003 / PY32F030
#############################################################################

else


CFILES += Libraries/CMSIS/Device/PY32F0xx/Source/system_py32f0xx.c



ifeq ($(USE_LL_LIB),y)


CDIRS += Libraries/PY32F0xx_LL_Driver/Src

INCLUDES += Libraries/PY32F0xx_LL_Driver/Inc

LIB_FLAGS += USE_FULL_LL_DRIVER


else


CDIRS += Libraries/PY32F0xx_HAL_Driver/Src

INCLUDES += Libraries/PY32F0xx_HAL_Driver/Inc


endif


EXCLUDE_SOURCES :=



ifneq (,$(findstring PY32F002A,$(LIB_FLAGS)))

AFILES := \
Libraries/CMSIS/Device/PY32F0xx/Source/gcc/startup_py32f002a.s

endif



ifneq (,$(findstring PY32F003,$(LIB_FLAGS)))

AFILES := \
Libraries/CMSIS/Device/PY32F0xx/Source/gcc/startup_py32f003.s

endif



ifneq (,$(findstring PY32F030,$(LIB_FLAGS)))

AFILES := \
Libraries/CMSIS/Device/PY32F0xx/Source/gcc/startup_py32f030.s

endif


endif



#############################################################################
# Additional libraries
#############################################################################


ifeq ($(USE_FREERTOS),y)


CDIRS += \
    Libraries/FreeRTOS \
    Libraries/FreeRTOS/portable/GCC/ARM_CM0


CFILES += \
    Libraries/FreeRTOS/portable/MemMang/heap_4.c


INCLUDES += \
    Libraries/FreeRTOS/include \
    Libraries/FreeRTOS/portable/GCC/ARM_CM0


LIB_FLAGS += USE_RTOS


endif



ifeq ($(USE_DSP),y)


INCLUDES += \
    Libraries/CMSIS/DSP/Include \
    Libraries/CMSIS/DSP/PrivateInclude


LIB_FLAGS += USE_DSP


endif



ifeq ($(USE_EPAPER),y)


CDIRS += \
    Libraries/EPaper/Lib \
    Libraries/EPaper/Examples \
    Libraries/EPaper/Fonts \
    Libraries/EPaper/GUI


INCLUDES += \
    Libraries/EPaper/Lib \
    Libraries/EPaper/Examples \
    Libraries/EPaper/Fonts \
    Libraries/EPaper/GUI


endif



#############################################################################

include ./rules.mk
