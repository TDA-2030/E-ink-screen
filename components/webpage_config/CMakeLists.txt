idf_component_register(SRC_DIRS "." "cgi" "adapt"
                        INCLUDE_DIRS "include"
                        PRIV_REQUIRES wpa_supplicant esp_http_server mdns
                        EMBED_FILES "html/cfg_favicon.ico"
                                    "html/medley.min.js"
                                    "html/connecting.html"
                                    "html/style.css"
                                    "html/wifi.png"
                                    "html/wifi.html")