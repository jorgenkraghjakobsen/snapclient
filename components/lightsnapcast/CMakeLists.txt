idf_component_register(SRCS "snapcast.c" "player.c"
                       INCLUDE_DIRS "include"
                       REQUIRES libbuffer json libmedian audio_board)
