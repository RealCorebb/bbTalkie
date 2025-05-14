set(requires
    hardware_driver
    player
    esp_wifi
    nvs_flash
    )

idf_component_register(SRCS main.c
                       REQUIRES ${requires}
                       PRIV_REQUIRES esp32-spi-ssd1327
                       )

