cmake_minimum_required(VERSION 3.15)
project(mqtt_tcp C)

# =====================================================================
# 出力ファイル名 (cmake .. -DAPP_NAME=your_name で変更可能)
#   実行ファイルは常に <APP_NAME>.exe として生成される (全OS共通)
# =====================================================================
set(
    APP_NAME
    "mqtt_tcp"
    CACHE STRING "Output executable base name (without extension)"
)

# 全プラットフォームで .exe 拡張子に統一する
set(CMAKE_EXECUTABLE_SUFFIX ".exe")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# =====================================================================
# ソース
# =====================================================================
set(APP_SOURCES
    main.c
)

set(MQTTLIB_SOURCES
    lib/mqttlib.c
)

set(MQTT_SOURCES
    mqtt/mqtt_message_create.c
    mqtt/mqtt_client.c
)

# =====================================================================
# 実行ファイルのビルド
# =====================================================================
add_executable(${APP_NAME}
    ${APP_SOURCES}
    ${MQTTLIB_SOURCES}
    ${MQTT_SOURCES}
)

# =====================================================================
# インクルードパス
# =====================================================================
target_include_directories(${APP_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# =====================================================================
# コンパイルオプション
# =====================================================================
target_compile_options(${APP_NAME} PRIVATE
    $<$<C_COMPILER_ID:GNU>:    -Wall -Wextra -Wpedantic -Wno-unused-parameter>
    $<$<C_COMPILER_ID:Clang>:  -Wall -Wextra -Wpedantic -Wno-unused-parameter>
    $<$<C_COMPILER_ID:AppleClang>: -Wall -Wextra -Wpedantic -Wno-unused-parameter>
    # MSVC: /utf-8 でソース・実行文字セットを UTF-8 に統一
    #       → BOM なし UTF-8 ソースでの C4819 警告を解消
    $<$<C_COMPILER_ID:MSVC>: /W4 /WX /utf-8>
)

# =====================================================================
# コンパイル定義
# =====================================================================
target_compile_definitions(${APP_NAME} PRIVATE
    $<$<CONFIG:Debug>:MQTT_DEBUG_ENABLE>
    $<$<PLATFORM_ID:Windows>:_WINDOWS>
    $<$<NOT:$<PLATFORM_ID:Windows>>:_GNU_SOURCE>
)

# =====================================================================
# リンクライブラリ (Windows のみ Winsock が必要)
# =====================================================================
target_link_libraries(${APP_NAME} PRIVATE
    $<$<PLATFORM_ID:Windows>:ws2_32>
)

# =====================================================================
# ビルド設定サマリ
# =====================================================================
message(STATUS "")
message(STATUS "=== mqtt_tcp build configuration ===")
message(STATUS "  Output name : ${APP_NAME}.exe")
message(STATUS "  Build type  : ${CMAKE_BUILD_TYPE}")
message(STATUS "=====================================")
message(STATUS "")
message(STATUS "Build:           mkdir build && cd build && cmake .. && make")
message(STATUS "Debug build:     cmake .. -DCMAKE_BUILD_TYPE=Debug")
message(STATUS "Custom name:     cmake .. -DAPP_NAME=your_name")
message(STATUS "")
