set(SRCS
    "src/main.c"
    "src/provisioning.c"
    "src/rc_gpio.c"
    "src/supabase.c"
    "src/utils.c"
    "src/wifi.c"
    "src/zt_lora.c"
    "src/crash_report.c"
    "src/ota.c"
    "src/ota_selftest.c"
)

idf_component_register(SRCS "${SRCS}"
                       INCLUDE_DIRS "inc")