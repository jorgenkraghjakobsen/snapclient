file(GLOB srcs "flac/src/libFLAC/*.c")

file(GLOB srcs "flac/src/libFLAC/bitmath.c"
               "flac/src/libFLAC/bitreader.c"
			   "flac/src/libFLAC/cpu.c"
			   "flac/src/libFLAC/crc.c"
			   "flac/src/libFLAC/fixed.c"
			   "flac/src/libFLAC/float.c"
			   "flac/src/libFLAC/format.c"
			   "flac/src/libFLAC/lpc.c"
			   "flac/src/libFLAC/md5.c"
			   "flac/src/libFLAC/memory.c"
			   "flac/src/libFLAC/stream_decoder.c"
			   "flac/src/libFLAC/window.c"
			   "flac/src/libFLAC/crc.c" )

idf_component_register(SRCS "${srcs}" 
                       INCLUDE_DIRS .
					                "flac/src/libFLAC/include"
									"flac/include"
									)
target_compile_definitions(${COMPONENT_TARGET} PRIVATE "-DHAVE_CONFIG_H")
