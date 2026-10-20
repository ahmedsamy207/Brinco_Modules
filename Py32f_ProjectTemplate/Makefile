##### Project ###############################################################

PROJECT ?= Py32f_ProjectTemplate

# Generated files
TEMP_DIR  := Temp
BUILD_DIR := Build


##### MCU ###################################################################

# MCU types:
# PY32F002Ax5
# PY32F002Bx5
# PY32F003x4, PY32F003x6, PY32F003x8
# PY32F030x6, PY32F030x8
# PY32F040x6, PY32F040x8, PY32F040xB
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

PYOCD_EXE ?= pyocd


# OS detection
ifeq ($(OS),Windows_NT)
    HOST_OS := Windows
else
    HOST_OS := Unix
endif


# Convert MCU name
ifeq ($(HOST_OS),Windows)

    JLINK_DEVICE := $(shell powershell -Command "'$(MCU_TYPE)'.ToUpper()")
    PYOCD_DEVICE := $(shell powershell -Command "'$(MCU_TYPE)'.ToLower()")

else

    JLINK_DEVICE := $(shell echo $(MCU_TYPE) | tr '[:lower:]' '[:upper:]')
    PYOCD_DEVICE := $(shell echo $(MCU_TYPE) | tr '[:upper:]' '[:lower:]')

endif



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

LDSCRIPT = Libraries/LDScripts/$(shell echo $(MCU_TYPE) | tr '[:upper:]' '[:lower:]').ld



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
