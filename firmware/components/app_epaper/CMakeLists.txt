
set(SOURCE_FILES 
app_epaper.c
epdpaint.c
font/font8.c
font/font12.c
font/font16.c
font/font20.c
font/font24.c
font/font16gbk.c
font/cc936.c

)

idf_component_register(SRCS ${SOURCE_FILES}
                       INCLUDE_DIRS font .
                       REQUIRES driver)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-format)
