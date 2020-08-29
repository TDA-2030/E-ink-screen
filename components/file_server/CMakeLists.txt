idf_component_register(SRC_DIRS "."
                    INCLUDE_DIRS "include"
                    EMBED_FILES "favicon.ico" "upload_script.html"
                    PRIV_REQUIRES fatfs spiffs esp_http_server)