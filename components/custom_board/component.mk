#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

ifdef CONFIG_AUDIO_BOARD_CUSTOM
COMPONENT_ADD_INCLUDEDIRS += ./generic_board/include
COMPONENT_SRCDIRS += ./generic_board

ifdef CONFIG_DAC_PCM51XX
COMPONENT_ADD_INCLUDEDIRS += ./pcm51xx/include
COMPONENT_SRCDIRS += ./pcm51xx
endif

ifdef CONFIG_DAC_MA120X0
COMPONENT_ADD_INCLUDEDIRS += ./ma120x0/include
COMPONENT_SRCDIRS += ./ma120x0
endif

endif
