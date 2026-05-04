# mqtt_tcp

IoT機器向け **MQTT over TCP** クライアント実装。  
外部ライブラリへの依存は**ゼロ**。標準ライブラリ・OS API は `lib/mqttlib.h` / `lib/mqttlib.c` に完全集約しており、移植時の差し替えコストを最小化しています。  
Ubuntu / Linux および Windows (MinGW / MSVC) での動作確認済み。

---

## 概要

```
アプリケーション層   main.c
                        │
    MQTT制御層      mqtt/mqtt_client.c           (CONNECT / PUBLISH / SUBSCRIBE シーケンス)
                    mqtt/mqtt_message_create.c   (MQTTパケット生成)
                        │
    TCPトランスポート層  lib/mqttlib.c            (mq_tcp_connect / send / recv / close)
    OS API集約          lib/mqttlib.h            (全ソースが include する唯一の標準ライブラリ窓口)
```

---

## ディレクトリ構成

```
mqtt_tcp/
├── main.c                          # エントリポイント (pub / sub モード)
├── CMakeLists.txt                  # ビルド設定
│
├── lib/
│   ├── mqttlib.h                   # 標準ライブラリ・OS API 集約ヘッダ (移植窓口)
│   └── mqttlib.c                   # TCP ソケット実装 (移植時の差し替え対象)
│
└── mqtt/
    ├── mqtt_client.h               # MQTT over TCP API
    ├── mqtt_client.c               # MQTT送受信・接続シーケンス
    ├── mqtt_message_create.h       # MQTTパケット生成API
    └── mqtt_message_create.c       # MQTTパケット生成実装
```

---

## 要件

### Ubuntu / Linux

```bash
sudo apt install -y build-essential cmake
```

追加の外部ライブラリは**不要**です。

### Windows

MinGW (MSYS2) または Visual Studio (MSVC) が使用できます。

```bash
# MSYS2 MinGW64 の場合
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake
```

Windows では `ws2_32` (Winsock2) が自動リンクされます。

---

## ビルド

### CMake を使う場合

```bash
cd mqtt_tcp
mkdir build && cd build
cmake ..
make -j$(nproc)        # Linux / MinGW
# cmake --build .      # MSVC / クロスプラットフォーム共通
```

ビルドが成功すると `build/mqtt_tcp` (Linux) または `build/mqtt_tcp.exe` (Windows) が生成されます。

### デバッグビルド

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

### gcc で直接ビルドする場合 (CMake なし)

```bash
gcc -std=c11 -Wall -Wextra -I. \
    main.c lib/mqttlib.c mqtt/mqtt_message_create.c mqtt/mqtt_client.c \
    -o mqtt_tcp
```

Windows (MinGW) の場合は末尾に `-lws2_32` を追加してください。

---

## 接続先ブローカーの設定

`main.c` 内の定数を書き換えます。

```c
const char *host = "127.0.0.1"; /* ブローカーのホスト名 or IPアドレス */
int         port = 1883;        /* MQTT 標準ポート */
```

または `mqtt_publisher()` / `mqtt_subscriber()` を直接呼び出す場合は引数で渡します。

---

## 実行

### パブリッシャー

```bash
./mqtt_tcp pub <topic> <message> [clientID]
```

### サブスクライバー

```bash
./mqtt_tcp sub <topic> [clientID]
```

### 引数

| 引数 | 説明 |
|------|------|
| `pub` / `sub` | 動作モード |
| `topic` | MQTTトピック名 |
| `message` | 送信メッセージ (pub のみ必須) |
| `clientID` | MQTTクライアントID (省略時: `mqttTcpPub` / `mqttTcpSub`) |

### 使用例

```bash
# メッセージを1件パブリッシュ
./mqtt_tcp pub mqttTest "Hello from mqtt_tcp"

# クライアントIDを指定してパブリッシュ
./mqtt_tcp pub mqttTest "Hello" myDevice

# トピックを購読 (タイムアウトまで受信し続ける)
./mqtt_tcp sub mqttTest

# クライアントIDを指定して購読
./mqtt_tcp sub mqttTest mySubscriber
```

### Windows 環境の注意点

Windowsターミナルのデフォルト文字コードは CP932 のため、UTF-8 メッセージを正しく表示するには以下を実行してください。

```cmd
chcp 65001
```

---

## API リファレンス

### OS API 集約レイヤ (`lib/mqttlib.h`)

全ソースファイルはこのヘッダのみを `#include` します。  
標準ライブラリ関数のラッパーはすべて `static inline` で提供しており、ランタイムコストはゼロです。

| カテゴリ | 主な関数 |
|---------|---------|
| メモリ操作 | `mq_memcpy` / `mq_memmove` / `mq_memset` / `mq_memcmp` |
| 文字列操作 | `mq_strlen` / `mq_strncpy` / `mq_strcmp` / `mq_strdup` |
| 動的メモリ | `mq_malloc` / `mq_calloc` / `mq_realloc` / `mq_free` |
| フォーマット出力 | `mq_snprintf` / `mq_printf` / `mq_fprintf` |
| 時刻 | `mq_gettimeofday` / `mq_clock_gettime` |
| プロセス制御 | `mq_abort` / `mq_exit` |
| TCP ソケット | `mq_tcp_connect` / `mq_tcp_send` / `mq_tcp_recv` / `mq_tcp_recv_timeout` / `mq_tcp_close` |

#### TCP ソケット API

```c
/*
 * TCPソケットを生成してサーバへ接続する
 *   timeout_ms : 接続タイムアウト [ms]。0 = OS デフォルト
 *   戻り値     : 成功時=有効なソケット, 失敗時=MQ_INVALID_SOCKET
 */
mq_socket_t mq_tcp_connect(const char *host, int port, int timeout_ms);

/*
 * ソケットへバイト列を確実に送信する
 *   戻り値 : 0=成功, -1=エラー
 */
int mq_tcp_send(mq_socket_t sock, const uint8_t *buf, size_t len);

/*
 * ソケットから正確に len バイト受信する (タイムアウトなし)
 *   戻り値 : 0=成功, -1=エラー/切断
 */
int mq_tcp_recv(mq_socket_t sock, uint8_t *buf, size_t len);

/*
 * タイムアウト付きで len バイト受信する
 *   戻り値 : 0=成功, -1=エラー/切断, 1=タイムアウト
 */
int mq_tcp_recv_timeout(mq_socket_t sock, uint8_t *buf, size_t len, int timeout_ms);

/*
 * ソケットを閉じる
 */
void mq_tcp_close(mq_socket_t sock);
```

---

### MQTT制御層 (`mqtt/mqtt_client.h`)

#### セッション管理

```c
/* セッション構造体: 確立済みの TCP ソケットを保持する */
typedef struct {
    mq_socket_t sock; /* MQ_INVALID_SOCKET = 未接続 */
} mqtt_session_t;

#define MQTT_SESSION_INIT { MQ_INVALID_SOCKET }
```

#### 高レベルAPI

```c
/* パブリッシャー: TCP接続 → CONNECT → PUBLISH → DISCONNECT */
int mqtt_publisher(const char *host, int port, int debug,
                   const mqtt_publisher_options_t *opts);

/* サブスクライバー: TCP接続 → CONNECT → SUBSCRIBE → 受信ループ */
int mqtt_subscriber(const char *host, int port, int debug, int loop,
                    const mqtt_subscriber_options_t *opts,
                    mqtt_message_t **out_msg);
```

`mqtt_publisher_options_t` の主なフィールド:

| フィールド | デフォルト | 説明 |
|-----------|-----------|------|
| `topic` | `"mqttTest"` | パブリッシュ先トピック名 |
| `message` | `"Hello from MQTT/TCP client!"` | 送信メッセージ |
| `client_id` | `"mqttTcpClient"` | MQTTクライアントID |
| `qos` | `0` | QoSレベル (0/1/2) |
| `retain` | `0` | `1` = RETAIN フラグ有効 |
| `debug` | `0` | `1` = デバッグ出力有効 |

`mqtt_subscriber_options_t` の主なフィールド:

| フィールド | デフォルト | 説明 |
|-----------|-----------|------|
| `topic` | `"mqttTest"` | 購読するトピック名 |
| `client_id` | `"mqttTcpClient"` | MQTTクライアントID |
| `qos` | `0` | QoSレベル (0/1/2) |
| `timeout_ms` | `30000` | 受信タイムアウト (ms) |
| `debug` | `0` | `1` = デバッグ出力有効 |

#### 低レベルAPI

```c
/* TCP接続 + CONNECT → CONNACK */
int mqtt_connection(const char *host, int port, const char *client_id,
                    const mqtt_connection_options_t *opts,
                    mqtt_session_t *session);

/* PUBLISH 送信 */
int mqtt_data_send(mqtt_session_t *session, const char *topic,
                   const char *payload, const mqtt_send_options_t *opts);

/* SUBSCRIBE 送信 + SUBACK 受信 (初回1回だけ呼ぶ) */
int mqtt_subscribe(mqtt_session_t *session, const char *topic, int qos,
                   const mqtt_recv_options_t *opts);

/* PUBLISH を1件受信する (loop で繰り返し呼び出す) */
int mqtt_recv_message(mqtt_session_t *session, mqtt_message_t **out_msg,
                      const mqtt_recv_options_t *opts);

/* DISCONNECT 送信 + ソケットクローズ */
void mqtt_disconnect(mqtt_session_t *session);

/* 受信メッセージの解放 */
void mqtt_message_free(mqtt_message_t *msg);
```

#### 低レベルAPIの使用例

```c
/* パブリッシュ */
mqtt_session_t sess = MQTT_SESSION_INIT;
mqtt_connection("broker.example.com", 1883, "myDevice", NULL, &sess);

mqtt_send_options_t send_opts = MQTT_SEND_OPTIONS_DEFAULT;
mqtt_data_send(&sess, "sensors/temp", "25.3", &send_opts);
mqtt_disconnect(&sess);

/* サブスクライブ */
mqtt_session_t sess = MQTT_SESSION_INIT;
mqtt_connection("broker.example.com", 1883, "mySubscriber", NULL, &sess);
mqtt_subscribe(&sess, "sensors/temp", 0, NULL);

mqtt_message_t *msg = NULL;
while (mqtt_recv_message(&sess, &msg, NULL) == 0) {
    printf("%s : %s\n", msg->topic, (char *)msg->payload);
    mqtt_message_free(msg);
    msg = NULL;
}
mqtt_disconnect(&sess);
```

---

### MQTTパケット生成層 (`mqtt/mqtt_message_create.h`)

```c
/* CONNECT パケット生成 (MQTT 5.0) */
int mqtt_connect_message(mqtt_packet_t *pkt, const char *client_id,
                         const mqtt_connect_options_t *opts);

/* PUBLISH パケット生成 (MQTT 5.0, QoS 0/1/2) */
int mqtt_publish_message(mqtt_packet_t *pkt, const char *topic,
                         const char *payload, const mqtt_publish_options_t *opts);

/* SUBSCRIBE パケット生成 (MQTT 5.0) */
int mqtt_subscribe_message(mqtt_packet_t *pkt, const char *topic,
                           const mqtt_subscribe_options_t *opts);

/* DISCONNECT パケット生成 (MQTT 5.0) */
int mqtt_disconnect_message(mqtt_packet_t *pkt);
```

---

## TCP上のMQTTパケット受信方式

TCPはバイトストリームのため、パケット境界は自前で判定します。  
`mqtt_client.c` 内の `recv_mqtt_packet()` が以下の3ステップで1パケットを組み立てます。

```
Step 1: Fixed Header の1バイト目を受信 (パケット種別)
Step 2: Variable Byte Integer (Remaining Length) を最大4バイト逐次受信
Step 3: Remaining Length 分の残バイト列を受信
```

バッファサイズの上限は `MQTT_TCP_BUF_SIZE`（デフォルト 65536 バイト）で制御します。

---

## IoTマイコン (Raspberry Pi Pico / ESP32) への移植

すべての標準ライブラリ・OS API 呼び出しは `lib/mqttlib.h` / `lib/mqttlib.c` の2ファイルに集約されています。移植は原則としてこの2ファイルだけを差し替えることで完了します。

- `mq_malloc` / `mq_free` を RTOS のヒープ (`pvPortMalloc` / `vPortFree`) に差し替える
- `mq_tcp_connect` / `mq_tcp_send` / `mq_tcp_recv_timeout` / `mq_tcp_close` を lwIP の TCP API (`tcp_connect` 等) または RTOS ソケット実装に差し替える
- `mq_printf` / `mq_fprintf` を UART 出力や no-op に差し替える
- タイムアウトなし受信が不要であれば `mq_tcp_recv` も同様に差し替える

暗号処理・TLS は不要です（TCP 自体に暗号機能はなく、TLS を追加したい場合は別途 mbedtls 等を組み込んでください）。

---

## 依存関係

外部ライブラリへの依存は**ゼロ**です。

| 種別 | 内容 |
|------|------|
| 標準ライブラリ | `string.h` / `stdlib.h` / `stdio.h` / `stdint.h` 等 |
| POSIX | `sys/socket.h` / `poll.h` / `netdb.h` / `unistd.h` 等 |
| Windows | `winsock2.h` / `ws2tcpip.h` (自動リンク: `ws2_32`) |
