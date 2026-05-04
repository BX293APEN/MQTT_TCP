/*
 * mqtt_client.h
 *
 * MQTT over TCP クライアント — 送受信・接続制御レイヤ
 *
 * 責務分担:
 *   mqtt_message_create.c/h : MQTTパケットのバイト列「生成」のみ
 *   mqtt_client.c/h         : TCPソケットを使ったMQTT over TCPの
 *                             データ送受信・接続シーケンス制御
 *                             (CONNECT/PUBLISH/SUBSCRIBE/パーサー)
 *   lib/mqttlib.h/c         : 標準ライブラリ・TCP ソケット抽象化レイヤ
 *
 * 主要API:
 *   mqtt_connection()        ブローカへ接続 (CONNECT→CONNACK)
 *   mqtt_data_send()         ブローカへデータを送る (PUBLISH送信)
 *   mqtt_subscribe()         SUBSCRIBEを送信しSUBACKを受信する
 *   mqtt_recv_message()      PUBLISHメッセージを1件受信する
 *   mqtt_disconnect()        ブローカへ切断通知を送る
 *   mqtt_publisher()         高レベルAPI: 1件PUBLISHして終了
 *   mqtt_subscriber()        高レベルAPI: SUBSCRIBEしてメッセージを受信
 */

#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

/* すべての標準型・OS API は mqttlib.h 経由で提供する */
#include "../lib/mqttlib.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===== デフォルト定数 ===== */

#define MQTT_CLIENT_DEFAULT_KEEPALIVE   60
#define MQTT_CLIENT_RECV_TIMEOUT_MS     5000
#define MQTT_CLIENT_CONNECT_TIMEOUT_MS  5000

/* ===== パース済みパケット構造体 ===== */

/** CONNACK パケット解析結果 */
typedef struct {
    uint8_t reason_code;     /* 0x00 = Success */
    int     session_present; /* セッション継続フラグ */
} mqtt_connack_t;

/** SUBACK パケット解析結果 */
typedef struct {
    uint16_t packet_id;     /* 対応するSUBSCRIBEのパケットID */
    uint8_t  reason_code;   /* 0x00 = Granted QoS 0 など */
} mqtt_suback_t;

/** 受信メッセージ (PUBLISH パケット解析結果) */
typedef struct {
    char     topic[256];    /* トピック名 */
    uint8_t *payload;       /* ペイロード (malloc済み。使用後は mqtt_message_free() で解放) */
    size_t   payload_len;   /* ペイロードのバイト数 */
    int      qos;           /* QoSレベル */
    uint16_t packet_id;     /* QoS 0 では 0 */
} mqtt_message_t;

/* ===== 接続オプション ===== */

typedef struct {
    int debug;        /* 1=デバッグ出力有効 */
    int keep_alive;   /* Keep Alive 秒数 (0=デフォルト60秒) */
    int clean_start;  /* 1=Clean Start フラグ (デフォルト1) */
} mqtt_connection_options_t;

#define MQTT_CONNECTION_OPTIONS_DEFAULT { 0, 60, 1 }

/* ===== 送信オプション ===== */

typedef struct {
    int qos;    /* QoSレベル 0/1/2 (デフォルト0) */
    int retain; /* 1=RETAINフラグ有効 (デフォルト0) */
    int debug;  /* 1=デバッグ出力有効 */
} mqtt_send_options_t;

#define MQTT_SEND_OPTIONS_DEFAULT { 0, 0, 0 }

/* ===== 受信オプション ===== */

typedef struct {
    int timeout_ms; /* 受信タイムアウト (ms)。0=MQTT_CLIENT_RECV_TIMEOUT_MS を使用 */
    int debug;      /* 1=デバッグ出力有効 */
} mqtt_recv_options_t;

#define MQTT_RECV_OPTIONS_DEFAULT { 0, 0 }

/* ===== セッション管理 ===== */

/**
 * MQTTセッション: 確立済みのTCPソケットを保持する。
 *
 * 使用例:
 *   mqtt_session_t sess = MQTT_SESSION_INIT;
 *   mqtt_connection("broker", 1883, "myClient", NULL, &sess);
 *   mqtt_data_send(&sess, "topic", "hello", NULL);
 *   mqtt_disconnect(&sess);
 */
typedef struct {
    mq_socket_t sock; /* TCPソケット (MQ_INVALID_SOCKET=未接続) */
} mqtt_session_t;

#define MQTT_SESSION_INIT { MQ_INVALID_SOCKET }

/* ==========================================================================
 * 公開API — 低レベル
 * ========================================================================== */

/**
 * ブローカへ接続する (TCP接続 + CONNECT → CONNACK)
 *
 * TCPソケットを生成してブローカへ接続し、MQTTの CONNECT パケットを送信して
 * CONNACK を受信することで MQTT セッションを確立する。
 * 確立したソケットは session に保存され、以後の送受信に使用する。
 *
 * @param[in]  host      サーバーホスト名 または IPアドレス
 * @param[in]  port      ポート番号
 * @param[in]  client_id MQTTクライアントID
 * @param[in]  opts      接続オプション (NULLのときデフォルト値を使用)
 * @param[out] session   確立したソケットを保存するセッション構造体
 * @return 0=成功, 負=エラー
 */
int mqtt_connection(
    const char                      *host,
    int                              port,
    const char                      *client_id,
    const mqtt_connection_options_t *opts,
    mqtt_session_t                  *session
);

/**
 * ブローカへデータを送る (PUBLISH 送信)
 *
 * @param[in] session  確立済みセッション
 * @param[in] topic    パブリッシュ先トピック名
 * @param[in] payload  送信ペイロード文字列
 * @param[in] opts     送信オプション (NULLのときデフォルト値を使用)
 * @return 0=成功, 負=エラー
 */
int mqtt_data_send(
    mqtt_session_t           *session,
    const char               *topic,
    const char               *payload,
    const mqtt_send_options_t *opts
);

/**
 * SUBSCRIBE パケットを送信して SUBACK を受信する (初回1回だけ呼ぶ)
 *
 * @param[in] session  確立済みセッション
 * @param[in] topic    購読するトピック名
 * @param[in] qos      QoSレベル (0/1/2)
 * @param[in] opts     受信オプション (NULLのときデフォルト値を使用)
 * @return 0=成功, 負=エラー
 */
int mqtt_subscribe(
    mqtt_session_t            *session,
    const char                *topic,
    int                        qos,
    const mqtt_recv_options_t *opts
);

/**
 * PUBLISH メッセージを1件受信して out_msg に保存する
 *
 * PUBLISH 以外のパケットは自動的にスキップする。
 * mqtt_subscribe() でSUBSCRIBE済みのセッションに対して繰り返し呼び出す。
 * 返ったメッセージは必ず mqtt_message_free() で解放すること。
 *
 * @param[in]  session  確立済みセッション
 * @param[out] out_msg  受信メッセージの格納先ポインタ
 * @param[in]  opts     受信オプション (NULLのときデフォルト値を使用)
 * @return 0=成功, 1=タイムアウト, 負=エラー/切断
 */
int mqtt_recv_message(
    mqtt_session_t            *session,
    mqtt_message_t           **out_msg,
    const mqtt_recv_options_t *opts
);

/**
 * ブローカへ切断通知を送り、TCPソケットを閉じる
 *
 * @param[in] session  切断するセッション
 */
void mqtt_disconnect(mqtt_session_t *session);

/**
 * mqtt_recv_message() が返したメッセージを解放する
 *
 * @param[in] msg 解放するメッセージポインタ (NULLでも安全)
 */
void mqtt_message_free(mqtt_message_t *msg);

/* ==========================================================================
 * 高レベルAPI: パブリッシャー / サブスクライバー
 * ========================================================================== */

/** パブリッシャーオプション */
typedef struct {
    const char *topic;     /* パブリッシュ先トピック名 (デフォルト: "mqttTest") */
    const char *message;   /* 送信メッセージ           (デフォルト: "Hello from MQTT/TCP client!") */
    const char *client_id; /* MQTTクライアントID       (デフォルト: "mqttTcpClient") */
    int         qos;       /* QoSレベル 0/1/2          (デフォルト: 0) */
    int         retain;    /* 1=RETAINフラグ有効       (デフォルト: 0) */
    int         debug;     /* 1=デバッグ出力有効       (デフォルト: 0) */
} mqtt_publisher_options_t;

#define MQTT_PUBLISHER_OPTIONS_DEFAULT \
    { "mqttTest", "Hello from MQTT/TCP client!", "mqttTcpClient", 0, 0, 0 }

/** サブスクライバーオプション */
typedef struct {
    const char *topic;      /* 購読するトピック名 (デフォルト: "mqttTest") */
    const char *client_id;  /* MQTTクライアントID (デフォルト: "mqttTcpClient") */
    int         qos;        /* QoSレベル 0/1/2    (デフォルト: 0) */
    int         timeout_ms; /* 受信タイムアウト(ms)(デフォルト: 30000) */
    int         debug;      /* 1=デバッグ出力有効  (デフォルト: 0) */
} mqtt_subscriber_options_t;

#define MQTT_SUBSCRIBER_OPTIONS_DEFAULT \
    { "mqttTest", "mqttTcpClient", 0, 30000, 0 }

/**
 * パブリッシャー: TCPに接続してMQTT CONNECTを行い、メッセージを1件PUBLISHして終了する。
 *
 * @param[in] host    接続先サーバーアドレス
 * @param[in] port    接続先ポート番号
 * @param[in] debug   1=デバッグ出力有効、0=無効
 * @param[in] opts    送信オプション (NULLのときデフォルト値を使用)
 * @return 0=成功, 非0=エラー
 */
int mqtt_publisher(
    const char                    *host,
    int                            port,
    int                            debug,
    const mqtt_publisher_options_t *opts
);

/**
 * サブスクライバー: TCPに接続してMQTT CONNECTを行い、SUBSCRIBEして
 * メッセージを受信する。loop=1 の場合はタイムアウト/切断まで受信し続ける。
 *
 * 受信したメッセージは out_msg が指すアドレスに都度格納される。
 * メッセージ使用後は必ず mqtt_message_free(*out_msg) で解放すること。
 *
 * @param[in]  host      接続先サーバーアドレス
 * @param[in]  port      接続先ポート番号
 * @param[in]  debug     1=デバッグ出力有効、0=無効
 * @param[in]  loop      1=タイムアウト/切断まで受信し続ける, 0=1件受信したら終了
 * @param[in]  opts      受信オプション (NULLのときデフォルト値を使用)
 * @param[out] out_msg   受信メッセージの格納先アドレス (loop=0 のとき有効)
 *                       (使用後は mqtt_message_free() で解放すること)
 * @return 0=正常終了, 非0=エラー
 */
int mqtt_subscriber(
    const char                      *host,
    int                              port,
    int                              debug,
    int                              loop,
    const mqtt_subscriber_options_t *opts,
    mqtt_message_t                 **out_msg
);

/* ==========================================================================
 * パーサー (内部利用・テスト用に公開)
 * ========================================================================== */

/**
 * 受信バイト列から CONNACK をパースする
 */
int mqtt_parse_connack(const uint8_t *data, size_t data_len, mqtt_connack_t *out);

/**
 * 受信バイト列から SUBACK をパースする
 */
int mqtt_parse_suback(const uint8_t *data, size_t data_len, mqtt_suback_t *out);

/**
 * 受信バイト列から PUBLISH をパースする
 * @return malloc済みのmqtt_message_t* (パース失敗時はNULL)
 *         返ったポインタは必ず mqtt_message_free() で解放すること。
 */
mqtt_message_t *mqtt_parse_publish(const uint8_t *data, size_t data_len);

#ifdef __cplusplus
}
#endif

#endif /* MQTT_CLIENT_H */
