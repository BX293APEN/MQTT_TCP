/*
 * mqttlib.h  —  MQTT over TCP ポータブル標準ライブラリ抽象化レイヤ
 *
 * 目的:
 *   Windows / Linux / macOS など様々な環境で動作する
 *   MQTT over TCP プロジェクト全体の標準ライブラリ・OS API 呼び出しを
 *   このヘッダ一本に集約し、移植時の差し替えコストを最小化する。
 *
 *   各ソースファイルは <string.h> / <stdlib.h> / <stdio.h> 等を直接
 *   include せず、このヘッダのみを include する。
 *   実装の差し替えは lib/mqttlib.c だけ行えばよい。
 *
 * 対象:
 *   メモリ操作  : mq_memcpy / mq_memmove / mq_memset /
 *                 mq_memcmp / mq_memchr
 *   文字列操作  : mq_strlen / mq_strnlen / mq_strncpy /
 *                 mq_strncmp / mq_strcmp / mq_strchr / mq_strdup
 *   動的メモリ  : mq_malloc / mq_calloc / mq_realloc / mq_free
 *   フォーマット: mq_snprintf / mq_vsnprintf / mq_sprintf
 *   ログ出力    : mq_fprintf / mq_printf
 *   時刻        : mq_gettimeofday / mq_clock_gettime
 *   プロセス制御: mq_abort / mq_exit
 *   TCP ソケット: mq_tcp_connect / mq_tcp_send / mq_tcp_recv /
 *                 mq_tcp_recv_timeout / mq_tcp_close
 */

#ifndef MQTTLIB_H
#define MQTTLIB_H

/* POSIX 拡張 (strnlen / strdup / clock_gettime / getaddrinfo / poll 等) を有効にする */
#ifndef _WIN32
#  ifndef _POSIX_C_SOURCE
#    define _POSIX_C_SOURCE 200809L
#  endif
#  ifndef _DEFAULT_SOURCE
#    define _DEFAULT_SOURCE 1
#  endif
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

/* -----------------------------------------------------------------------
 * 文字列・メモリ・動的確保
 * ----------------------------------------------------------------------- */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* -----------------------------------------------------------------------
 * 時刻
 * ----------------------------------------------------------------------- */
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <time.h>
#  ifndef CLOCK_REALTIME
#    define CLOCK_REALTIME  0
#  endif
#  ifndef CLOCK_MONOTONIC
#    define CLOCK_MONOTONIC 1
#  endif
#else
#  include <sys/time.h>
#  include <time.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <netdb.h>
#  include <unistd.h>
#  include <fcntl.h>
#  include <errno.h>
#  include <poll.h>
#endif

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * メモリ操作ラッパー
 * ========================================================================== */

static inline void *mq_memcpy(void *dst, const void *src, size_t n)
    { return memcpy(dst, src, n); }

static inline void *mq_memmove(void *dst, const void *src, size_t n)
    { return memmove(dst, src, n); }

static inline void *mq_memset(void *s, int c, size_t n)
    { return memset(s, c, n); }

static inline int mq_memcmp(const void *s1, const void *s2, size_t n)
    { return memcmp(s1, s2, n); }

static inline void *mq_memchr(const void *s, int c, size_t n)
    { return memchr(s, c, n); }

/* ==========================================================================
 * 文字列操作ラッパー
 * ========================================================================== */

static inline size_t mq_strlen(const char *s)
    { return strlen(s); }

static inline size_t mq_strnlen(const char *s, size_t maxlen)
    { return strnlen(s, maxlen); }

static inline char *mq_strncpy(char *dst, const char *src, size_t n)
    { return strncpy(dst, src, n); }

static inline int mq_strncmp(const char *s1, const char *s2, size_t n)
    { return strncmp(s1, s2, n); }

static inline int mq_strcmp(const char *s1, const char *s2)
    { return strcmp(s1, s2); }

static inline char *mq_strchr(const char *s, int c)
    { return strchr(s, c); }

static inline char *mq_strdup(const char *s)
    { return strdup(s); }

/* ==========================================================================
 * 動的メモリラッパー
 * ========================================================================== */

static inline void *mq_malloc(size_t size)
    { return malloc(size); }

static inline void *mq_calloc(size_t nmemb, size_t size)
    { return calloc(nmemb, size); }

static inline void *mq_realloc(void *ptr, size_t size)
    { return realloc(ptr, size); }

static inline void mq_free(void *ptr)
    { free(ptr); }

/* ==========================================================================
 * フォーマット出力ラッパー
 * ========================================================================== */

static inline int mq_snprintf(char *str, size_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(str, size, fmt, ap);
    va_end(ap);
    return r;
}

static inline int mq_vsnprintf(char *str, size_t size, const char *fmt, va_list ap)
    { return vsnprintf(str, size, fmt, ap); }

static inline int mq_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vprintf(fmt, ap);
    va_end(ap);
    return r;
}

static inline int mq_fprintf(FILE *stream, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vfprintf(stream, fmt, ap);
    va_end(ap);
    return r;
}

/* ==========================================================================
 * プロセス制御ラッパー
 * ========================================================================== */

static inline void mq_abort(void) { abort(); }
static inline void mq_exit(int code) { exit(code); }

/* ==========================================================================
 * 時刻ラッパー
 * ========================================================================== */

static inline int mq_gettimeofday(struct timeval *tv)
{
#ifdef _WIN32
    /* Windows: GetSystemTimeAsFileTime → timeval 変換 */
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t t = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    t -= 116444736000000000ULL; /* 1601→1970 */
    tv->tv_sec  = (long)(t / 10000000ULL);
    tv->tv_usec = (long)((t % 10000000ULL) / 10);
    return 0;
#else
    return gettimeofday(tv, NULL);
#endif
}

static inline int mq_clock_gettime(int clk_id, struct timespec *ts)
{
    (void)clk_id;
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t t = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    t -= 116444736000000000ULL;
    ts->tv_sec  = (time_t)(t / 10000000ULL);
    ts->tv_nsec = (long)((t % 10000000ULL) * 100);
    return 0;
#else
    return clock_gettime((clockid_t)clk_id, ts);
#endif
}

/* ==========================================================================
 * TCP ソケット抽象化 API
 * ========================================================================== */

/*
 * mq_socket_t: プラットフォーム共通ソケット型
 *   Windows  : SOCKET  (unsigned 整数)
 *   POSIX    : int     (ファイルディスクリプタ)
 */
#ifdef _WIN32
typedef SOCKET mq_socket_t;
#  define MQ_INVALID_SOCKET  INVALID_SOCKET
#  define MQ_SOCKET_ERROR    SOCKET_ERROR
#else
typedef int    mq_socket_t;
#  define MQ_INVALID_SOCKET  ((mq_socket_t)(-1))
#  define MQ_SOCKET_ERROR    (-1)
#endif

/*
 * mq_tcp_connect() — TCPソケットを生成してサーバへ接続する
 *
 * @param[in]  host       サーバホスト名 または IPアドレス文字列
 * @param[in]  port       ポート番号 (1–65535)
 * @param[in]  timeout_ms 接続タイムアウト [ms]。0 = OS デフォルト
 * @return     成功時: 有効なソケット記述子, 失敗時: MQ_INVALID_SOCKET
 */
mq_socket_t mq_tcp_connect(const char *host, int port, int timeout_ms);

/*
 * mq_tcp_send() — ソケットへバイト列を確実に送信する
 *
 * @param[in]  sock      送信先ソケット
 * @param[in]  buf       送信バッファ
 * @param[in]  len       送信バイト数
 * @return     0=成功, -1=エラー
 */
int mq_tcp_send(mq_socket_t sock, const uint8_t *buf, size_t len);

/*
 * mq_tcp_recv() — ソケットから正確に len バイト受信する (タイムアウトなし)
 *
 * @param[in]  sock   受信元ソケット
 * @param[out] buf    受信バッファ
 * @param[in]  len    受信するバイト数
 * @return     0=成功, -1=エラー/切断
 */
int mq_tcp_recv(mq_socket_t sock, uint8_t *buf, size_t len);

/*
 * mq_tcp_recv_timeout() — タイムアウト付きで len バイト受信する
 *
 * @param[in]  sock       受信元ソケット
 * @param[out] buf        受信バッファ
 * @param[in]  len        受信するバイト数
 * @param[in]  timeout_ms タイムアウト [ms]。0 = 無制限
 * @return     0=成功, -1=エラー/切断, 1=タイムアウト
 */
int mq_tcp_recv_timeout(mq_socket_t sock, uint8_t *buf, size_t len, int timeout_ms);

/*
 * mq_tcp_close() — ソケットを閉じる
 *
 * @param[in]  sock  閉じるソケット
 */
void mq_tcp_close(mq_socket_t sock);

#ifdef __cplusplus
}
#endif

#endif /* MQTTLIB_H */
