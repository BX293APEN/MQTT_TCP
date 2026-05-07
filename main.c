/*
 * main.c
 *
 * MQTT over TCP クライアント — エントリポイント
 *
 * 使い方:
 *   pub: ./mqtt_tcp pub <topic> <message> [client_id]
 *   sub: ./mqtt_tcp sub <topic> [client_id]
 *
 * デバッグ出力は以下のビルド定義で制御する:
 *   (現状はコード内の debug フラグで制御。必要に応じて -DMQTT_DEBUG をどうぞ)
 *
 * 移植:
 *   Windows: _WIN32 が自動定義される。
 *            ws2_32.lib / advapi32.lib のリンクが必要。
 *   Linux / macOS: 追加リンクなし。
 */

#include "lib/mqttlib.h"
#include "mqtt/mqtt_client.h"

int main(int argc, char **argv)
{
#ifdef _WIN32
    WSADATA wsaData = { 0 };
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    if (argc < 3) {
        mq_fprintf(stderr,
            "Usage:\n"
            "  %s pub <topic> <message> [client_id]\n"
            "  %s sub <topic> [client_id]\n",
            argv[0], argv[0]);
        return 1;
    }

    const char *host   = "192.168.10.64"; /* 接続先ブローカーアドレス */
    int         port   = 1883;        /* MQTT 標準ポート */
    const char *mode   = argv[1];
    const char *topic  = argv[2];
    int         debug  = 0;
    int         ret    = 1;

    if (mq_strcmp(mode, "pub") == 0 && argc >= 4) {
        const char *message   = argv[3];
        const char *client_id = (argc >= 5) ? argv[4] : "mqttTcpPub";

        mqtt_publisher_options_t pub_opts = MQTT_PUBLISHER_OPTIONS_DEFAULT;
        pub_opts.topic     = topic;
        pub_opts.message   = message;
        pub_opts.client_id = client_id;
        pub_opts.qos       = 0;
        pub_opts.retain    = 1;

        ret = mqtt_publisher(host, port, debug, &pub_opts);

    } else if (mq_strcmp(mode, "sub") == 0) {
        const char *client_id = (argc >= 4) ? argv[3] : "mqttTcpSub";
        int         loop      = 1; /* タイムアウト/切断まで受信し続ける */

        mqtt_subscriber_options_t sub_opts = MQTT_SUBSCRIBER_OPTIONS_DEFAULT;
        sub_opts.topic      = topic;
        sub_opts.client_id  = client_id;
        sub_opts.qos        = 0;
        sub_opts.timeout_ms = 30000;

        mq_fprintf(stderr,
            "[main] sub server=%s:%d topic=%s loop=%d\n",
            host, port, topic, loop);

        mqtt_message_t *msg = NULL;
        ret = mqtt_subscriber(host, port, debug, loop, &sub_opts, &msg);

        /* loop=0 で1件受信した場合 */
        if (msg) {
            mq_printf("%s : %.*s\n",
                      msg->topic,
                      (int)msg->payload_len,
                      msg->payload ? (char *)msg->payload : "");
            mqtt_message_free(msg);
        }

    } else {
        mq_fprintf(stderr,
            "[main] Unknown mode: \"%s\". Use \"pub\" or \"sub\".\n", mode);
        ret = 1;
    }

#ifdef _WIN32
    WSACleanup();
#endif
    return ret;
}
