# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(IDF_PATH)/components/soc/src/esp32 $(IDF_PATH)/components/soc/src/esp32/include $(IDF_PATH)/components/soc/include $(IDF_PATH)/components/soc/soc/include $(IDF_PATH)/components/soc/soc/esp32 $(IDF_PATH)/components/soc/soc/esp32/include
COMPONENT_LDFLAGS += -L$(BUILD_DIR_BASE)/soc -lsoc
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += soc
COMPONENT_LDFRAGMENTS += $(IDF_PATH)/components/soc/linker.lf
component-soc-build: 
