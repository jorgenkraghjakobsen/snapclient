# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(IDF_PATH)/components/freertos/include $(IDF_PATH)/components/freertos/xtensa/include
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/freertos -lfreertos -Wl,--undefined=uxTopUsedPriority
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += freertos
COMPONENT_LDFRAGMENTS += $(IDF_PATH)/components/freertos/linker.lf
component-freertos-build: 
