#
# Component Makefile
#
COMPONENT_ADD_INCLUDEDIRS := include include/silk include/silk/celt include/silk/fixed include/celt 

COMPONENT_SRCDIRS := library library/celt library/silk library/silk/fixed 

CFLAGS += -Wno-unused-function -DHAVE_CONFIG_H -Os -DSMALL_FOOTPRINT -funroll-loops -ffast-math
