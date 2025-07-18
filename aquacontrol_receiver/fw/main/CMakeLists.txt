set(component_srcs "src/main.c" "src/utils.c" "src/rc_gpio.c" "src/supabase.c" "src/provisioning.c")

idf_component_register(SRCS "src/utils.c" "${component_srcs}"
                       INCLUDE_DIRS "./inc")


