set(component_srcs "main.c"
                   "utils.c"
                   "rtcwake.c"
)

idf_component_register(SRCS "rtcwake.c" "utils.c" "${component_srcs}"
                       INCLUDE_DIRS ".")


