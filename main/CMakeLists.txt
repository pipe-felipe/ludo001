# Generate wifi_credentials.h from the project's .env file (gitignored,
# never committed) so Wi-Fi credentials don't end up hardcoded in source.
set(ENV_FILE "${CMAKE_SOURCE_DIR}/.env")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GENERATED_DIR}")

set(WIFI_SSID "")
set(WIFI_PASSWORD "")
if(EXISTS "${ENV_FILE}")
    file(STRINGS "${ENV_FILE}" ENV_LINES)
    foreach(LINE ${ENV_LINES})
        if(LINE MATCHES "^WIFI_SSID=\"?([^\"]*)\"?$")
            set(WIFI_SSID "${CMAKE_MATCH_1}")
        elseif(LINE MATCHES "^WIFI=\"?([^\"]*)\"?$")
            set(WIFI_PASSWORD "${CMAKE_MATCH_1}")
        endif()
    endforeach()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${ENV_FILE}")
endif()

file(WRITE "${GENERATED_DIR}/wifi_credentials.h"
"#pragma once\n#define WIFI_SSID \"${WIFI_SSID}\"\n#define WIFI_PASSWORD \"${WIFI_PASSWORD}\"\n")

idf_component_register(SRCS "ludo001.cpp"
                    INCLUDE_DIRS "." "${GENERATED_DIR}"
                    PRIV_REQUIRES wifi_manager oled_display sd_card web_server)
