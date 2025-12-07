# ESP-NOW Radio Control CMakeLists.txt
# Unified sender/receiver with runtime configuration

set(COMMON_SOURCES
    "main.c"
    "shared.c"
    "sender.c"
    "receiver.c"
    "settings.c"
    "webserver.c"
)

idf_component_register(SRCS ${COMMON_SOURCES})
