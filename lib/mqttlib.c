/*
 * mqttlib.c  —  MQTT over TCP ポータブル標準ライブラリ実装
 *
 * mqttlib.h で宣言した TCP ソケット API の実装。
 * メモリ・文字列操作はヘッダのインライン関数で完結しているため、
 * ここでは TCP 接続 / 送受信 / クローズのみ実装する。
 *
 * 移植:
 *   Windows: _WIN32 を定義してビルドする。
 *            Winsock2 を使用 (ws2_32.lib のリンクが必要)。
 *   POSIX  : デフォルト。Linux / macOS / BSDs で動作する。
 */

#include "mqttlib.h"

/* ==========================================================================
 * 内部ヘルパー: ソケットをノンブロッキング / ブロッキングに切り替える
 * ========================================================================== */

#ifdef _WIN32
static int set_nonblocking(mq_socket_t sock, int enable)
{
    u_long mode = enable ? 1 : 0;
    return ioctlsocket(sock, FIONBIO, &mode) == 0 ? 0 : -1;
}
#else
static int set_nonblocking(mq_socket_t sock, int enable)
{
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) return -1;
    if (enable)
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;
    return fcntl(sock, F_SETFL, flags) == 0 ? 0 : -1;
}
#endif

/* ==========================================================================
 * 内部ヘルパー: poll / select で書き込み可能になるまで待つ (接続確認用)
 * ========================================================================== */

static int wait_writable(mq_socket_t sock, int timeout_ms)
{
#ifdef _WIN32
    fd_set wfds, efds;
    FD_ZERO(&wfds); FD_SET(sock, &wfds);
    FD_ZERO(&efds); FD_SET(sock, &efds);
    struct timeval tv;
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    int r = select(0, NULL, &wfds, &efds, timeout_ms > 0 ? &tv : NULL);
    if (r <= 0) return -1;
    if (FD_ISSET(sock, &efds)) return -1;
    /* 接続エラー確認 */
    int err = 0; int errlen = sizeof(err);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, (char *)&err, &errlen);
    return err == 0 ? 0 : -1;
#else
    struct pollfd pfd = { sock, POLLOUT, 0 };
    int r = poll(&pfd, 1, timeout_ms > 0 ? timeout_ms : -1);
    if (r <= 0) return -1;
    if (pfd.revents & (POLLERR | POLLHUP)) return -1;
    /* 接続エラー確認 */
    int err = 0; socklen_t errlen = sizeof(err);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &errlen);
    return err == 0 ? 0 : -1;
#endif
}

/* ==========================================================================
 * mq_tcp_connect
 * ========================================================================== */

mq_socket_t mq_tcp_connect(const char *host, int port, int timeout_ms)
{
    if (!host || port <= 0 || port > 65535)
        return MQ_INVALID_SOCKET;

#ifdef _WIN32
    /* Winsock 初期化は呼び出し側 (main) で行うこと */
#endif

    /* ホスト名解決 */
    char port_str[8];
    mq_snprintf(port_str, sizeof(port_str), "%d", port);

    struct addrinfo hints;
    mq_memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;   /* IPv4 / IPv6 両対応 */
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo *res = NULL;
    if (getaddrinfo(host, port_str, &hints, &res) != 0 || res == NULL)
        return MQ_INVALID_SOCKET;

    mq_socket_t sock = MQ_INVALID_SOCKET;

    for (struct addrinfo *p = res; p != NULL; p = p->ai_next) {
        sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock == MQ_INVALID_SOCKET) continue;

        if (timeout_ms > 0) {
            /* ノンブロッキングで connect し、poll/select で完了を待つ */
            if (set_nonblocking(sock, 1) != 0) {
                mq_tcp_close(sock);
                sock = MQ_INVALID_SOCKET;
                continue;
            }

#ifdef _WIN32
            int r = connect(sock, p->ai_addr, (int)p->ai_addrlen);
            int in_progress = (r == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK);
#else
            int r = connect(sock, p->ai_addr, p->ai_addrlen);
            int in_progress = (r < 0 && errno == EINPROGRESS);
#endif

            if (r == 0 || in_progress) {
                if (in_progress && wait_writable(sock, timeout_ms) != 0) {
                    mq_tcp_close(sock);
                    sock = MQ_INVALID_SOCKET;
                    continue;
                }
                /* ブロッキングに戻す */
                set_nonblocking(sock, 0);
                break;
            }
            mq_tcp_close(sock);
            sock = MQ_INVALID_SOCKET;
        } else {
            /* タイムアウトなし: 通常のブロッキング connect */
#ifdef _WIN32
            int r = connect(sock, p->ai_addr, (int)p->ai_addrlen);
#else
            int r = connect(sock, p->ai_addr, p->ai_addrlen);
#endif
            if (r == 0) break;
            mq_tcp_close(sock);
            sock = MQ_INVALID_SOCKET;
        }
    }

    freeaddrinfo(res);
    return sock;
}

/* ==========================================================================
 * mq_tcp_send — 指定バイト数を確実に送信する
 * ========================================================================== */

int mq_tcp_send(mq_socket_t sock, const uint8_t *buf, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
#ifdef _WIN32
        int n = send(sock, (const char *)(buf + sent), (int)(len - sent), 0);
        if (n == SOCKET_ERROR) return -1;
#else
        ssize_t n = send(sock, buf + sent, len - sent, MSG_NOSIGNAL);
        if (n <= 0) return -1;
#endif
        sent += (size_t)n;
    }
    return 0;
}

/* ==========================================================================
 * mq_tcp_recv — 指定バイト数を確実に受信する (タイムアウトなし)
 * ========================================================================== */

int mq_tcp_recv(mq_socket_t sock, uint8_t *buf, size_t len)
{
    size_t recvd = 0;
    while (recvd < len) {
#ifdef _WIN32
        int n = recv(sock, (char *)(buf + recvd), (int)(len - recvd), 0);
        if (n <= 0) return -1;
#else
        ssize_t n = recv(sock, buf + recvd, len - recvd, 0);
        if (n <= 0) return -1;
#endif
        recvd += (size_t)n;
    }
    return 0;
}

/* ==========================================================================
 * mq_tcp_recv_timeout — タイムアウト付き受信
 *
 * 戻り値: 0=成功, -1=エラー/切断, 1=タイムアウト
 * ========================================================================== */

int mq_tcp_recv_timeout(mq_socket_t sock, uint8_t *buf, size_t len, int timeout_ms)
{
    size_t recvd = 0;

    while (recvd < len) {
        if (timeout_ms > 0) {
            /* poll/select で受信可能になるまで待つ */
#ifdef _WIN32
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(sock, &rfds);
            struct timeval tv;
            tv.tv_sec  = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;
            int r = select(0, &rfds, NULL, NULL, &tv);
            if (r == 0)  return 1;  /* タイムアウト */
            if (r < 0)   return -1; /* エラー */
#else
            struct pollfd pfd = { sock, POLLIN, 0 };
            int r = poll(&pfd, 1, timeout_ms);
            if (r == 0)  return 1;  /* タイムアウト */
            if (r < 0)   return -1; /* エラー */
            if (pfd.revents & (POLLERR | POLLHUP)) return -1;
#endif
        }

#ifdef _WIN32
        int n = recv(sock, (char *)(buf + recvd), (int)(len - recvd), 0);
        if (n <= 0) return -1;
#else
        ssize_t n = recv(sock, buf + recvd, len - recvd, 0);
        if (n <= 0) return -1;
#endif
        recvd += (size_t)n;
    }
    return 0;
}

/* ==========================================================================
 * mq_tcp_close
 * ========================================================================== */

void mq_tcp_close(mq_socket_t sock)
{
    if (sock == MQ_INVALID_SOCKET) return;
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}
