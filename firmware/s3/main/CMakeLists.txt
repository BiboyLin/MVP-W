idf_component_register(
    SRCS
        "app_main.c"
        "ws_router.c"
        "uart_bridge.c"
        "button_voice.c"
        "display_ui.c"
        "hal_audio.c"
        "hal_display.c"
        "hal_uart.c"
        "hal_opus.c"
        "wifi_client.c"
        "ws_client.c"
        "cJSON.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_netif esp_event driver nvs_flash sensecap-watcher
)
