# Recursively collect all source files in src/
file(GLOB_RECURSE SOURCES
        main.cpp
#        "src/algorithms/*.c"
        "src/algorithms/*.cpp"
        "src/core/*.cpp"
        "src/devices/*.cpp"
)

set(INC_DIRS
        "src/devices"
#        "src/bt"
        "src/algorithms"
        "src/core"
)

#set(SOURCES main.cpp "src")
idf_component_register(SRCS ${SOURCES}
        INCLUDE_DIRS ${INC_DIRS}
        REQUIRES i2c_device nvs_flash esp_driver_i2s esp_driver_gpio)

#idf_build_get_property(project_dir PROJECT_DIR)
#idf_build_set_property(CONFIG_KCONFIG_FILES "${project_dir}/main/Kconfig" APPEND)
#idf_build_set_property(CONFIG_KCONFIG_FILES "Kconfig" APPEND)