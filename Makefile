# 项目名称
TARGET = EliteV2_Scope

# 编译参数
DEBUG = 1

ifeq ($(DEBUG), 1)
    OPT = -Og
    CFLAGS_EXTRA = -g3 -gdwarf-2
else
    OPT = -O2 -ffast-math
    CFLAGS_EXTRA = 
endif

# 交叉编译器定义
PREFIX = arm-none-eabi-
CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size
HEX = $(CP) -O ihex
BIN = $(CP) -O binary -S

# 路径定义
BUILD_DIR = build

# 宏定义
C_DEFS =  \
-DSTM32F10X_HD \
-DUSE_STDPERIPH_DRIVER \
-DARM_MATH_CM3 \
-DDISABLEFLOAT16 \
-DARM_MATH_LOOPUNROLL

# 包含路径
C_INCLUDES = \
-IApp/inc \
-IBoard/inc \
-ILibraries/CMSIS \
-ILibraries/CMSIS/DSP/Include \
-ILibraries/CMSIS/DSP/PrivateInclude \
-ILibraries/StdPeriph/inc \
-IFreeRTOS/include \
-IFreeRTOS/portable/GCC/ARM_CM3 \
-IGUI \
-IGUI/lvgl \
-IGUI/lvgl/src \
-IGUI/lvgl/demos

# 源代码搜寻
C_SOURCES = $(shell find App -name "*.c") \
            $(shell find Board -name "*.c") \
            $(shell find Libraries/StdPeriph -name "*.c") \
            $(shell find Libraries/CMSIS -name "system_stm32f10x.c") \
            $(shell find FreeRTOS -maxdepth 1 -name "*.c") \
			$(shell find GUI/lvgl -name "*.c") \
			$(shell find Libraries/CMSIS/DSP/Source -name "*.c") \
            FreeRTOS/portable/GCC/ARM_CM3/port.c \
            FreeRTOS/portable/MemMang/heap_4.c

# 启动文件
AS_SOURCES = Project/startup/startup_stm32f10x_hd.s

# MCU 标志
CPU = -mcpu=cortex-m3
MCU = $(CPU) -mthumb

# 编译选项
CFLAGS = $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) $(CFLAGS_EXTRA) -Wall -fdata-sections -ffunction-sections

# 链接选项
LDSCRIPT = Project/stm32f103zet6.ld
LDFLAGS = $(MCU) -specs=nano.specs -T$(LDSCRIPT) $(OPT) $(CFLAGS_EXTRA) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections
LDFLAGS += -lm
# 编译目标
all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin

# 生成对象文件列表
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(AS_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(AS_SOURCES)))

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR) 
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf
	$(HEX) $< $@
	
$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf
	$(BIN) $< $@	

$(BUILD_DIR):
	mkdir $@		

clean:
	rm -fR $(BUILD_DIR)
  
.PHONY: all clean