set(component_srcs "main.c" "utils.c" "rc_gpio.c")

idf_component_register(SRCS "utils.c" "${component_srcs}"
                       INCLUDE_DIRS ".")


