# ESP-NOW Radio Control CMakeLists.txt
# Conditional compilation based on APP_ROLE_SENDER or APP_ROLE_RECEIVER

set(COMMON_SOURCES
    "main.c"
    "shared.c"
)

idf_build_get_property(compile_definitions COMPILE_DEFINITIONS)

if("APP_ROLE_SENDER=1" IN_LIST compile_definitions)
    message(STATUS "Building SENDER role")
    set(APP_SOURCES ${COMMON_SOURCES} "sender.c")
elseif("APP_ROLE_RECEIVER=1" IN_LIST compile_definitions)
    message(STATUS "Building RECEIVER role")
    set(APP_SOURCES ${COMMON_SOURCES} "receiver.c")
else()
    message(FATAL_ERROR "Please define APP_ROLE_SENDER or APP_ROLE_RECEIVER in build_flags")
endif()

idf_component_register(SRCS ${APP_SOURCES})
