/* mbedtls_user_config.h: curl.app's and the browser's changes to mbedTLS 3.6's default configuration
   (MBEDTLS_USER_CONFIG_FILE, read after include/mbedtls/mbedtls_config.h). The defaults stay, TLS 1.2 and 1.3 among
   them; out go what needs an operating system, the server side, DTLS and x86's instructions. MIT License, as
   cmm2img.h. */

/* the CMM2 has no BSD sockets of mbedTLS's kind, no POSIX timers and no file system for PSA's key store */
#undef MBEDTLS_NET_C
#undef MBEDTLS_TIMING_C
#undef MBEDTLS_PSA_CRYPTO_STORAGE_C
#undef MBEDTLS_PSA_ITS_FILE_C

/* entropy: the RP2350's true random number generator (port/cmm2_tls.c), not /dev/urandom */
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_ENTROPY_HARDWARE_ALT

/* milliseconds since start-up (port/cmm2_tls.c); time() and gmtime_r are newlib's on MMBasic's clock */
#define MBEDTLS_PLATFORM_MS_TIME_ALT

/* a client only: no TLS server, no DTLS */
#undef MBEDTLS_SSL_SRV_C
#undef MBEDTLS_SSL_CACHE_C
#undef MBEDTLS_SSL_COOKIE_C
#undef MBEDTLS_SSL_TICKET_C
#undef MBEDTLS_SSL_PROTO_DTLS
#undef MBEDTLS_SSL_DTLS_ANTI_REPLAY
#undef MBEDTLS_SSL_DTLS_HELLO_VERIFY
#undef MBEDTLS_SSL_DTLS_CLIENT_PORT_REUSE
#undef MBEDTLS_SSL_DTLS_CONNECTION_ID
#undef MBEDTLS_SSL_DTLS_SRTP

/* x86's and ARMv8's instructions: none on a Cortex-M33 */
#undef MBEDTLS_AESNI_C
#undef MBEDTLS_AESCE_C
#undef MBEDTLS_PADLOCK_C

/* size: the self tests and the debug messages */
#undef MBEDTLS_SELF_TEST
#undef MBEDTLS_DEBUG_C

/* records: 16 KB in, as servers send them (few honour Max Fragment Length); 4 KB out is enough for requests */
#define MBEDTLS_SSL_IN_CONTENT_LEN  16384
#define MBEDTLS_SSL_OUT_CONTENT_LEN 4096
