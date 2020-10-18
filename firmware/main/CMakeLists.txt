
idf_component_register(SRC_DIRS "."
                    INCLUDE_DIRS "." "include"
                    )

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