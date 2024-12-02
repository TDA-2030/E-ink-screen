# Register the component
idf_component_register(SRCS lp_environment_sensor.c file_manage.c app_show_image.c lv_port.c
                       INCLUDE_DIRS "."
                       )
#
# ULP support additions to component CMakeLists.txt.
#
# 1. The LP Core app name must be unique (if multiple components use LP Core).
set(ulp_app_name lp_core_${COMPONENT_NAME})
#
# 2. Specify all C files.
#    Files should be placed into a separate directory (in this case, lp_core/),
#    which should not be added to COMPONENT_SRCS.
set(ulp_lp_core_sources "lp_core/main.c")

#
# 3. List all the component source files which include automatically
#    generated LP Core export file, ${ulp_app_name}.h:
set(ulp_exp_dep_srcs "lp_environment_sensor.c")

#
# 4. Call function to build ULP binary and embed in project using the argument
#    values above.
ulp_embed_binary(${ulp_app_name} "${ulp_lp_core_sources}" "${ulp_exp_dep_srcs}")


set(IMAGE_DIR "../spiffs_image")
if(EXISTS ${IMAGE_DIR})
    # execute_process(COMMAND bash ${AUDIO_DIR}/is_update.sh voice ../build
    #                 WORKING_DIRECTORY "${AUDIO_DIR}"
    #                 RESULT_VARIABLE AUDIO_IS_UPDATED
    #                 )
    # if(AUDIO_IS_UPDATED EQUAL 27)
        spiffs_create_partition_image(storage ${IMAGE_DIR} FLASH_IN_PROJECT)
    # endif()
else()
    message(FATAL_ERROR "${IMAGE_DIR}/dist doesn't exit. ")
endif()