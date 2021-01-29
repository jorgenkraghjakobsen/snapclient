set(COMPONENT_REQUIRES)
set(COMPONENT_PRIV_REQUIRES audio_board audio_sal audio_hal esp-dsp)

list(APPEND COMPONENT_ADD_INCLUDEDIRS ./include)
set(COMPONENT_SRCS ./dsp_processor.c)
register_component()

# IDF >=4
idf_component_get_property(audio_board_lib audio_board COMPONENT_LIB)
set_property(TARGET ${audio_board_lib} APPEND PROPERTY INTERFACE_LINK_LIBRARIES ${COMPONENT_LIB})
