idf_component_register(SRCS "websocket.c" "websocket_server.c" 
                       INCLUDE_DIRS "include"
                       REQUIRES lwip mbedtls)
