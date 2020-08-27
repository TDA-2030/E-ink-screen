
# Only ESP32 has enough memory to do jpeg decoding
if(IDF_TARGET STREQUAL "esp32")
    list(APPEND srcs "decode_image.c")
endif()

idf_component_register(SRCS "app_main.cpp"
                    INCLUDE_DIRS "."
                    EMBED_FILES image.jpg)

