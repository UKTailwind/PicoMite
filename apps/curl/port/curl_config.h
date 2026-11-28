/* curl_config.h: libcurl 8.23.0 and the curl tool for the PicoMite (as the Colour Maximite 2's), written by hand (as curl's lib/config-*.h
   are) for a loadable program built with tools/sdk: newlib-nano, no operating system, the firmware's lwIP through
   services version 21 (port/cmm2_net.c gives the BSD socket calls on them), mbedTLS 3.6 for TLS and zlib for
   Content-Encoding. docs/Web_Browser_Study.md, step 1. MIT License, as cmm2img.h. */
#ifndef CMM2_CURL_CONFIG_H
#define CMM2_CURL_CONFIG_H

#define CURL_OS "arm-none-eabi-picomite"

/* the curl tool's main (src/tool_main.c), called by curl_main.c's: no other file of curl's has a main */
#define main curl_tool_main

/* the machine: Cortex-M33 (RP2350), ILP32 */
#define SIZEOF_INT              4
#define SIZEOF_LONG             4
#define SIZEOF_OFF_T            4
#define SIZEOF_CURL_OFF_T       8
#define SIZEOF_CURL_SOCKET_T    4
#define SIZEOF_SIZE_T           4
#define SIZEOF_TIME_T           8
#define HAVE_SUSECONDS_T        1
#define HAVE_STRUCT_TIMEVAL     1
#define HAVE_BOOL_T             1
#define HAVE_STDBOOL_H          1
#define HAVE_SA_FAMILY_T        1
#define HAVE_STRUCT_SOCKADDR_STORAGE 1

/* headers: newlib's, and port/'s for the network (sys/socket.h, netinet/in.h, netinet/tcp.h, arpa/inet.h, netdb.h) */
#define HAVE_ARPA_INET_H        1
#define HAVE_FCNTL_H            1
#define HAVE_LIBGEN_H           1
#define HAVE_LOCALE_H           1
#define HAVE_NETDB_H            1
#define HAVE_NETINET_IN_H       1
#define HAVE_NETINET_TCP_H      1
#define HAVE_STRINGS_H          1
#define HAVE_SYS_SELECT_H       1
#define HAVE_SYS_TYPES_H        1
#define HAVE_SYS_PARAM_H        1
#define HAVE_UNISTD_H           1
#define HAVE_DIRENT_H           1
#define HAVE_OPENDIR            1

/* functions: newlib's, tools/sdk/syscalls.c's and port/cmm2_net.c's */
#define HAVE_CLOCK_GETTIME_MONOTONIC 1
#define HAVE_CLOSESOCKET        1       /* port/cmm2_net.c: a socket's descriptor is not a file's */
#define HAVE_FREEADDRINFO       1
#define HAVE_GETADDRINFO        1
#define HAVE_GETADDRINFO_THREADSAFE 1
#define HAVE_GETPEERNAME        1
#define HAVE_GETSOCKNAME        1
#define HAVE_GETTIMEOFDAY       1
#define HAVE_GMTIME_R           1
#define HAVE_LOCALTIME_R        1
#define HAVE_SETLOCALE          1
#define HAVE_SETSOCKOPT_SO_NONBLOCK 1   /* port/cmm2_net.c: setsockopt(SOL_SOCKET, SO_NONBLOCK) */
#define HAVE_SIGNAL             1
#define HAVE_SOCKET             1
#define HAVE_STRCASECMP         1
#define HAVE_FSEEKO             1
#define HAVE_DECL_FSEEKO        1

#define HAVE_RECV 1
#define RECV_TYPE_ARG1 int
#define RECV_TYPE_ARG2 void *
#define RECV_TYPE_ARG3 size_t
#define RECV_TYPE_ARG4 int
#define RECV_TYPE_RETV ssize_t

#define HAVE_SEND 1
#define SEND_TYPE_ARG1 int
#define SEND_TYPE_ARG2 void *
#define SEND_TYPE_ARG3 size_t
#define SEND_TYPE_ARG4 int
#define SEND_TYPE_RETV ssize_t

/* what libcurl is built with */
#define USE_MBEDTLS             1
#define HAVE_LIBZ               1
#define USE_MANUAL              1       /* curl --manual */
#define CURL_CA_BUNDLE          "A:/cacert.pem"   /* as the manual's WEB TLS CA example: curl's bundle */
#define CURL_DISABLE_CA_SEARCH  1

/* the protocols: HTTP and HTTPS, and file: for the card */
#define CURL_DISABLE_DICT       1
#define CURL_DISABLE_FTP        1
#define CURL_DISABLE_GOPHER     1
#define CURL_DISABLE_IMAP       1
#define CURL_DISABLE_LDAP       1
#define CURL_DISABLE_LDAPS      1
#define CURL_DISABLE_MQTT       1
#define CURL_DISABLE_POP3       1
#define CURL_DISABLE_RTSP       1
#define CURL_DISABLE_SMB        1
#define CURL_DISABLE_SMTP       1
#define CURL_DISABLE_TELNET     1
#define CURL_DISABLE_TFTP       1
#define CURL_DISABLE_WEBSOCKETS 1
#define CURL_DISABLE_IPFS       1

/* features with no use here, or that want what the CMM2 has not */
#define CURL_DISABLE_ALTSVC     1
#define CURL_DISABLE_AWS        1
#define CURL_DISABLE_BINDLOCAL  1
#define CURL_DISABLE_DOH        1
#define CURL_DISABLE_KERBEROS_AUTH 1
#define CURL_DISABLE_NEGOTIATE_AUTH 1
#define CURL_DISABLE_NETRC      1
#define CURL_DISABLE_SOCKETPAIR 1       /* no pipes or socketpair: curl_multi_wakeup is not used */
#define CURL_DISABLE_HTTPSIG    1

#endif
