file(GLOB srcs "opus/src/*.c")
file(GLOB silk "opus/silk/*.c")
file(GLOB celt "opus/celt/*.c")
file(GLOB fixed "opus/slik/fixed/*.c")
file(GLOB float "opus/slik/float/*.c")


idf_component_register(SRCS "${srcs}" "${silk}" "${celt}" "${fixed}" "${float}"
                       INCLUDE_DIRS .
					                "opus/include"
									"opus/silk"
									"opus/silk/fixed"
									"opus/silk/float"
									"opus/celt"
									)


target_compile_definitions(${COMPONENT_TARGET} PRIVATE "-DHAVE_CONFIG_H")
