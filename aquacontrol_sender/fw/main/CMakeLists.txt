set(component_srcs "src/main.c"
                   "src/utils.c"
                   "src/rtcwake.c"
                   "src/ble_ota.c"
)

idf_component_register(SRCS "src/rtcwake.c" "src/utils.c" "${component_srcs}"
                       INCLUDE_DIRS "./inc")


