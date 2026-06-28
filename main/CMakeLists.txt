idf_component_register(SRCS "app_main.c"
                       INCLUDE_DIRS "."
                       REQUIRES
                        esp_wifi
                        esp_event
                        esp_netif
                        nvs_flash
                        esp_http_client
                        json
                        mbedtls
                        driver
                        esp_driver_i2c
                        esp_driver_spi
                        esp_driver_gpio
                        )
