cmake_minimum_required(VERSION 3.15)
project(mqtt_tcp C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# -------------------------------------------------------------------------
# ソース
# -------------------------------------------------------------------------
set(SOURCES
    main.c
    lib/mqttlib.c
    mqtt/mqtt_message_create.c
    mqtt/mqtt_client.c
)

add_executable(mqtt_tcp ${SOURCES})

# -------------------------------------------------------------------------
# インクルードパス
# -------------------------------------------------------------------------
target_include_directories(mqtt_tcp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# -------------------------------------------------------------------------
# コンパイルオプション
# -------------------------------------------------------------------------
if(MSVC)
    target_compile_options(mqtt_tcp PRIVATE /W4 /WX)
else()
    target_compile_options(mqtt_tcp PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
    )
endif()

# -------------------------------------------------------------------------
# リンクライブラリ (Windows のみ Winsock が必要)
# -------------------------------------------------------------------------
if(WIN32)
    target_link_libraries(mqtt_tcp PRIVATE ws2_32)
endif()

# -------------------------------------------------------------------------
# デバッグビルド設定
# -------------------------------------------------------------------------
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(mqtt_tcp PRIVATE MQTT_DEBUG_ENABLE)
endif()
