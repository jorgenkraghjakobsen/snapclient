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

# besides generating probably more optimized code "-Os" also causes the compiler
# to closer examine the code and avoid false warnings (or errors through 
# -Werror) for maybe uninitialized variable usages.
# Those could also be removed by adding -Wno-maybe-uninitialized

target_compile_options(${COMPONENT_TARGET} PRIVATE "-Os")
# target_compile_options(${COMPONENT_TARGET} PRIVATE "-Wno-maybe-uninitialized")
