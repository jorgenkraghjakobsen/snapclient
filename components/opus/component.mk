#
# Main Makefile. This is basically the same as a component makefile.
#
# This Makefile should, at the very least, just include $(SDK_PATH)/make/component_common.mk. By default,
# this will take the sources in the src/ directory, compile them and link them into
# lib(subdirectory_name).a in the build directory. This behaviour is entirely configurable,
# please read the ESP-IDF documents if you need to do this.
#

COMPONENT_SRCDIRS := opus/src opus/silk opus/silk/fixed opus/celt
COMPONENT_ADD_INCLUDEDIRS := . opus/include opus/silk opus/silk/fixed opus/celt
CFLAGS += -DHAVE_CONFIG_H
