idf_component_register(SRCS "app_server.c" 
                            "app_wifi.c"
                            "app_led.c"
                             "main.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES "../web/index.html")
