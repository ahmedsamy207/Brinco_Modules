##### Project #####

# add the make file and the arm-none-eabi to the PATH
# or to the VSCode path using Ctrl + Shift + P then "Open User Settings (JSON)"

PROJECT			?= Py32f003_Ultrasonic
# The path for generated files
TEMP_DIR		= Temp
BUILD_DIR		= Build

# MCU types: 
#   PY32F002Ax5
#   PY32F002Bx5
#   PY32F003x4, PY32F003x6, PY32F003x8,
#   PY32F030x6, PY32F030x8, 
#   PY32F072xB
MCU_TYPE		= PY32F003x8

##### Options #####

# Enable Copy the vector table in the RAM
ENABLE_VECT_TAB_MIGRATION ?= n

# Use LL library instead of HAL, y:yes, n:no
USE_LL_LIB		?= n
# Enable printf float %f support, y:yes, n:no
# ENABLE_PRINTF_FLOAT	?= n
# Build with FreeRTOS, y:yes, n:no
USE_FREERTOS	?= n
# Build with CMSIS DSP functions, y:yes, n:no
USE_DSP			?= n
# Build with Waveshare e-paper lib, y:yes, n:no
USE_EPAPER		?= n
# Programmer, jlink or pyocd
FLASH_PROGRM	?= pyocd

##### Toolchains #######

ARM_TOOLCHAIN	?=

# path to JLinkExe
JLINKEXE		?=
# JLink device (Uppercases)
JLINK_DEVICE	?= $(shell powershell -Command "(echo $(MCU_TYPE) | ForEach-Object { $_.ToUpper() })")

# path to PyOCD
# Resolution order:
#   1. PYOCD_EXE from the command line or the environment
#   2. the project virtual environment created by Tools/setup-pyocd.ps1
#   3. the Python launcher ("py -m pyocd"), which also works when the per-user
#      Scripts folder (e.g. %APPDATA%\Python\Python314\Scripts) is not in PATH
# Put quotes inside the value if the path contains spaces, e.g.
#   make flash PYOCD_EXE="C:\Users\Ahmed Samy\AppData\Roaming\Python\Python314\Scripts\pyocd.exe"
# (the recipes use the value verbatim, without adding their own quotes)
PYOCD_VENV		:= $(wildcard .venv/Scripts/pyocd.exe)
PYOCD_EXE		?= $(if $(PYOCD_VENV),"$(PYOCD_VENV)",py -m pyocd)
# PyOCD device (Lowercases)
PYOCD_DEVICE 	?= $(shell powershell -Command "$$env:MCU_TYPE='$(MCU_TYPE)'; $$env:MCU_TYPE.ToLower()")

##### Paths ############

# C and CPP source folders
CDIRS		:= User\Src 
# Single C and CPP source files
CFILES		:= 
CPPFILES	:= 

# ASM source folders
ADIRS		:= User\Inc 
# Single ASM source files
AFILES		:= 

# Include paths
INCLUDES	:= Libraries/CMSIS/Core/Include \
			Libraries/CMSIS/Device/PY32F0xx/Include \
			$(ADIRS)

##### Library Paths ############

# Library flags
LIB_FLAGS		= $(MCU_TYPE)

ifneq ($(ENABLE_VECT_TAB_MIGRATION), y)
	LIB_FLAGS	+= FORBID_VECT_TAB_MIGRATION
endif

# Link descript file: 
LDSCRIPT		= Libraries/LDScripts/$(MCU_TYPE).ld

ifneq (,$(findstring PY32F002B,$(MCU_TYPE)))

# PY32F002B >>>
CFILES		+= Libraries/CMSIS/Device/PY32F0xx/Source/system_py32f002b.c

ifeq ($(USE_LL_LIB),y)
CDIRS		+= Libraries/PY32F002B_LL_Driver/Src \
		Libraries/PY32F002B_LL_BSP/Src
INCLUDES	+= Libraries/PY32F002B_LL_Driver/Inc \
		Libraries/PY32F002B_LL_BSP/Inc
LIB_FLAGS	+= USE_FULL_LL_DRIVER
else
CDIRS		+= Libraries/PY32F002B_HAL_Driver/Src \
		Libraries/PY32F002B_HAL_BSP/Src
INCLUDES	+= Libraries/PY32F002B_HAL_Driver/Inc \
		Libraries/PY32F002B_HAL_BSP/Inc
endif
# Startup file
AFILES	:= Libraries/CMSIS/Device/PY32F0xx/Source/gcc/startup_py32f002b.s
# PY32F002B <<<

else ifneq (,$(findstring PY32F07,$(MCU_TYPE)))

#  PY32F07x >>>
CFILES		+= Libraries/CMSIS/Device/PY32F0xx/Source/system_py32f07x.c

CDIRS		+= Libraries/PY32F07x_HAL_Driver/Src \
		Libraries/PY32F07x_HAL_BSP/Src
INCLUDES	+= Libraries/PY32F07x_HAL_Driver/Inc \
		Libraries/PY32F07x_HAL_BSP/Inc
LIB_FLAGS	+= USE_HAL_DRIVER
# Startup file
AFILES	:= Libraries/CMSIS/Device/PY32F0xx/Source/gcc/startup_py32f072.s
#  PY32F07 <<<

else

# PY32F002A,003,030 >>>
CFILES		+= Libraries/CMSIS/Device/PY32F0xx/Source/system_py32f0xx.c

ifeq ($(USE_LL_LIB),y)
CDIRS		+= Libraries/PY32F0xx_LL_Driver/Src
INCLUDES	+= Libraries/PY32F0xx_LL_Driver/Inc
LIB_FLAGS	+= USE_FULL_LL_DRIVER
else
CDIRS		+= Libraries/PY32F0xx_HAL_Driver/Src 
INCLUDES	+= Libraries/PY32F0xx_HAL_Driver/Inc 
endif
# Startup file
ifneq (,$(findstring PY32F002A,$(LIB_FLAGS)))
AFILES	:= Libraries/CMSIS/Device/PY32F0xx/Source/gcc/startup_py32f002a.s
endif
ifneq (,$(findstring PY32F003,$(LIB_FLAGS)))
AFILES	:= Libraries/CMSIS/Device/PY32F0xx/Source/gcc/startup_py32f003.s
endif
ifneq (,$(findstring PY32F030,$(LIB_FLAGS)))
AFILES	:= Libraries/CMSIS/Device/PY32F0xx/Source/gcc/startup_py32f030.s
endif
# PY32F002A,003,030 <<<

endif

######## Additional Libs ########

ifeq ($(USE_FREERTOS),y)
CDIRS		+= Libraries/FreeRTOS \
			Libraries/FreeRTOS/portable/GCC/ARM_CM0

CFILES		+= Libraries/FreeRTOS/portable/MemMang/heap_4.c

INCLUDES	+= Libraries/FreeRTOS/include \
			Libraries/FreeRTOS/portable/GCC/ARM_CM0
endif

ifeq ($(USE_DSP),y)
CFILES 		+= Libraries/CMSIS/DSP/Source/BasicMathFunctions/BasicMathFunctions.c \
		Libraries/CMSIS/DSP/Source/BayesFunctions/BayesFunctions.c \
		Libraries/CMSIS/DSP/Source/CommonTables/CommonTables.c \
		Libraries/CMSIS/DSP/Source/ComplexMathFunctions/ComplexMathFunctions.c \
		Libraries/CMSIS/DSP/Source/ControllerFunctions/ControllerFunctions.c \
		Libraries/CMSIS/DSP/Source/DistanceFunctions/DistanceFunctions.c \
		Libraries/CMSIS/DSP/Source/FastMathFunctions/FastMathFunctions.c \
		Libraries/CMSIS/DSP/Source/FilteringFunctions/FilteringFunctions.c \
		Libraries/CMSIS/DSP/Source/InterpolationFunctions/InterpolationFunctions.c \
		Libraries/CMSIS/DSP/Source/MatrixFunctions/MatrixFunctions.c \
		Libraries/CMSIS/DSP/Source/QuaternionMathFunctions/QuaternionMathFunctions.c \
		Libraries/CMSIS/DSP/Source/StatisticsFunctions/StatisticsFunctions.c \
		Libraries/CMSIS/DSP/Source/SupportFunctions/SupportFunctions.c \
		Libraries/CMSIS/DSP/Source/SVMFunctions/SVMFunctions.c \
		Libraries/CMSIS/DSP/Source/TransformFunctions/TransformFunctions.c
INCLUDES	+= Libraries/CMSIS/DSP/Include \
		Libraries/CMSIS/DSP/PrivateInclude
endif

ifeq ($(USE_EPAPER),y)
CDIRS		+= Libraries/EPaper/Lib \
			Libraries/EPaper/Examples \
			Libraries/EPaper/Fonts \
			Libraries/EPaper/GUI

INCLUDES	+= Libraries/EPaper/Lib \
			Libraries/EPaper/Examples \
			Libraries/EPaper/Fonts \
			Libraries/EPaper/GUI
endif

include ./rules.mk
