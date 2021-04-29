# Requires only ESP-IDF
#set(requires
#)

# register_component() is now deprecated
set(sources
    "ps_pwm.c"
)

idf_component_register(SRCS "${sources}"
                       #INCLUDE_DIRS "src"
                       #REQUIRES "${requires}"
)
