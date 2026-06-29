/* ============================================================================
 * WiFi.c - WiFi / lwIP / web runtime, split out of PicoMite.c
 *
 * Compiled only for WEB variants (WEB_SOURCES). Holds ProcessWeb (the lwIP
 * poll pump, called from PicoMite.c's CheckAbort/routinechecks), WebConnect,
 * the deferred async-error helpers and the TLS/NTP time handling. The lwIP/
 * cyw43 callbacks and WiFi init glue stay in PicoMite.c. Interface in net/WiFi.h.
 * ============================================================================ */
#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"
#include "WiFi.h"
#ifdef PICOMITEWEB
#include "lwipopts.h"
#include "pico/cyw43_arch.h"
#include "pico/cyw43_driver.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/dns.h"
#include "lwip/udp.h"
#include "lwip/altcp.h"
#include "lwip/altcp_tcp.h"
#ifdef PICOMITEWEB_TLS
#include "lwip/altcp_tls.h"
#include "mbedtls/platform_time.h"
#include "mbedtls/ssl.h"
#endif

    /* Deferred error for lwIP/cyw43 callbacks. See Hardware_Includes.h. */
    static volatile bool web_async_error_set = false;
    static char web_async_error_msg[STRINGSIZE];

    void web_async_set_error(const char *msg)
    {
        if (web_async_error_set)
            return; /* first error wins */
        strncpy(web_async_error_msg, msg, sizeof(web_async_error_msg) - 1);
        web_async_error_msg[sizeof(web_async_error_msg) - 1] = 0;
        web_async_error_set = true;
    }

    void web_async_check_error(void)
    {
        if (!web_async_error_set)
            return;
        web_async_error_set = false;
        /* Copy locally — error() longjmps and the buffer could be overwritten
           by a re-entered callback if it weren't cleared above first. */
        char msg[STRINGSIZE];
        strncpy(msg, web_async_error_msg, sizeof(msg) - 1);
        msg[sizeof(msg) - 1] = 0;
        error(msg);
    }

#ifdef PICOMITEWEB_TLS
    /* TimeOffsetToUptime (int64_t, declared in MM_Misc.h) is set by WEB NTP
       to the offset in seconds such that:
           unix_time_seconds = TimeOffsetToUptime + time_us_64() / 1000000
       When zero (no NTP run), the formula reduces to uptime in seconds —
       usable for TLS session freshness but NOT for X.509 cert validity
       (every cert will look like it expired in 1970). For CA-verified TLS,
       run WEB NTP first. */

    /* Seconds-since-epoch — installed as mbedtls_time via
       MBEDTLS_PLATFORM_TIME_MACRO in mbedtls_config.h. Signature uses
       long long to match MBEDTLS_PLATFORM_TIME_TYPE_MACRO. */
    long long picomite_mbedtls_time(long long *tp)
    {
        long long t = (long long)TimeOffsetToUptime + (long long)(time_us_64() / 1000000);
        if (tp)
            *tp = t;
        return t;
    }

    /* Milliseconds — used by mbedtls SSL session freshness checks. */
    mbedtls_ms_time_t mbedtls_ms_time(void)
    {
        if (TimeOffsetToUptime != 0)
            return (mbedtls_ms_time_t)((int64_t)TimeOffsetToUptime * 1000 + (int64_t)(time_us_64() / 1000));
        return (mbedtls_ms_time_t)(time_us_64() / 1000);
    }

    /* Shared client TLS config — created lazily on first WEB OPEN TLS CLIENT.
       Public so MMTCPclient.c can hand it to altcp_tls_new(). lwIP's
       altcp_tls_create_config_client_common allocates its own entropy/DRBG
       context (altcp_tls_entropy_rng) on first call, so we don't need to
       provide one. mbedtls allocations are routed through lwIP's heap
       (MEM_SIZE in lwipopts.h, sized to fit a TLS session). */
    struct altcp_tls_config *picomite_tls_client_config = NULL;
    static bool picomite_tls_verify_required = false;

    void picomite_tls_init(void)
    {
        /* Retained for ABI compatibility / future hooks. */
    }

    /* Lazily create the shared client TLS config.
       Returns NULL on failure — caller must error() from main-thread context.
       If WEB TLS CA hasn't been run, the config has NO peer-certificate
       verification: handshake is encrypted but unauthenticated (MITM possible).
       After WEB TLS CA loads a bundle, the config requires verification. */
    struct altcp_tls_config *picomite_tls_get_client_config(void)
    {
        if (picomite_tls_client_config == NULL)
        {
            picomite_tls_client_config = altcp_tls_create_config_client(NULL, 0);
        }
        return picomite_tls_client_config;
    }

    /* Replace the active client TLS config with one that verifies peer certs
       against the supplied PEM/DER CA bundle. ca_buf must be valid PEM
       (null-terminated, ca_len includes the terminator) or DER (binary).
       Returns 0 on success, non-zero on parse/setup failure. */
    int picomite_tls_set_ca(const unsigned char *ca_buf, size_t ca_len)
    {
        struct altcp_tls_config *new_cfg = altcp_tls_create_config_client(ca_buf, ca_len);
        if (new_cfg == NULL)
            return -1;
        /* Force REQUIRED — the lwIP default authmode is OPTIONAL which
           silently continues on verify failure. struct altcp_tls_config is
           private to altcp_tls_mbedtls.c; we rely on its first field being
           mbedtls_ssl_config (stable across lwIP versions) and cast through
           the struct pointer. If pico-sdk ever reorders that struct this
           will quietly mis-behave — first-field is the only available door. */
        mbedtls_ssl_conf_authmode((mbedtls_ssl_config *)new_cfg, MBEDTLS_SSL_VERIFY_REQUIRED);
        if (picomite_tls_client_config != NULL)
            altcp_tls_free_config(picomite_tls_client_config);
        picomite_tls_client_config = new_cfg;
        picomite_tls_verify_required = true;
        return 0;
    }

    /* Drop any loaded CA and revert to the no-verify default config. */
    void picomite_tls_clear_ca(void)
    {
        if (picomite_tls_client_config != NULL)
        {
            altcp_tls_free_config(picomite_tls_client_config);
            picomite_tls_client_config = NULL;
        }
        picomite_tls_verify_required = false;
    }

    bool picomite_tls_verify_is_required(void)
    {
        return picomite_tls_verify_required;
    }
#endif /* PICOMITEWEB_TLS */

    static const struct
    {
        const char *iso;
        uint32_t cyw43;
    } wifi_country_table[WIFI_COUNTRY_COUNT] = {
        [WIFI_COUNTRY_WORLDWIDE] = {"XX", CYW43_COUNTRY('X', 'X', 0)},
        [WIFI_COUNTRY_AUSTRALIA] = {"AU", CYW43_COUNTRY('A', 'U', 0)},
        [WIFI_COUNTRY_AUSTRIA] = {"AT", CYW43_COUNTRY('A', 'T', 0)},
        [WIFI_COUNTRY_BELGIUM] = {"BE", CYW43_COUNTRY('B', 'E', 0)},
        [WIFI_COUNTRY_BRAZIL] = {"BR", CYW43_COUNTRY('B', 'R', 0)},
        [WIFI_COUNTRY_CANADA] = {"CA", CYW43_COUNTRY('C', 'A', 0)},
        [WIFI_COUNTRY_CHILE] = {"CL", CYW43_COUNTRY('C', 'L', 0)},
        [WIFI_COUNTRY_CHINA] = {"CN", CYW43_COUNTRY('C', 'N', 0)},
        [WIFI_COUNTRY_COLOMBIA] = {"CO", CYW43_COUNTRY('C', 'O', 0)},
        [WIFI_COUNTRY_CZECH_REPUBLIC] = {"CZ", CYW43_COUNTRY('C', 'Z', 0)},
        [WIFI_COUNTRY_DENMARK] = {"DK", CYW43_COUNTRY('D', 'K', 0)},
        [WIFI_COUNTRY_ESTONIA] = {"EE", CYW43_COUNTRY('E', 'E', 0)},
        [WIFI_COUNTRY_FINLAND] = {"FI", CYW43_COUNTRY('F', 'I', 0)},
        [WIFI_COUNTRY_FRANCE] = {"FR", CYW43_COUNTRY('F', 'R', 0)},
        [WIFI_COUNTRY_GERMANY] = {"DE", CYW43_COUNTRY('D', 'E', 0)},
        [WIFI_COUNTRY_GREECE] = {"GR", CYW43_COUNTRY('G', 'R', 0)},
        [WIFI_COUNTRY_HONG_KONG] = {"HK", CYW43_COUNTRY('H', 'K', 0)},
        [WIFI_COUNTRY_HUNGARY] = {"HU", CYW43_COUNTRY('H', 'U', 0)},
        [WIFI_COUNTRY_ICELAND] = {"IS", CYW43_COUNTRY('I', 'S', 0)},
        [WIFI_COUNTRY_INDIA] = {"IN", CYW43_COUNTRY('I', 'N', 0)},
        [WIFI_COUNTRY_ISRAEL] = {"IL", CYW43_COUNTRY('I', 'L', 0)},
        [WIFI_COUNTRY_ITALY] = {"IT", CYW43_COUNTRY('I', 'T', 0)},
        [WIFI_COUNTRY_JAPAN] = {"JP", CYW43_COUNTRY('J', 'P', 0)},
        [WIFI_COUNTRY_KENYA] = {"KE", CYW43_COUNTRY('K', 'E', 0)},
        [WIFI_COUNTRY_LATVIA] = {"LV", CYW43_COUNTRY('L', 'V', 0)},
        [WIFI_COUNTRY_LIECHTENSTEIN] = {"LI", CYW43_COUNTRY('L', 'I', 0)},
        [WIFI_COUNTRY_LITHUANIA] = {"LT", CYW43_COUNTRY('L', 'T', 0)},
        [WIFI_COUNTRY_LUXEMBOURG] = {"LU", CYW43_COUNTRY('L', 'U', 0)},
        [WIFI_COUNTRY_MALAYSIA] = {"MY", CYW43_COUNTRY('M', 'Y', 0)},
        [WIFI_COUNTRY_MALTA] = {"MT", CYW43_COUNTRY('M', 'T', 0)},
        [WIFI_COUNTRY_MEXICO] = {"MX", CYW43_COUNTRY('M', 'X', 0)},
        [WIFI_COUNTRY_NETHERLANDS] = {"NL", CYW43_COUNTRY('N', 'L', 0)},
        [WIFI_COUNTRY_NEW_ZEALAND] = {"NZ", CYW43_COUNTRY('N', 'Z', 0)},
        [WIFI_COUNTRY_NIGERIA] = {"NG", CYW43_COUNTRY('N', 'G', 0)},
        [WIFI_COUNTRY_NORWAY] = {"NO", CYW43_COUNTRY('N', 'O', 0)},
        [WIFI_COUNTRY_PERU] = {"PE", CYW43_COUNTRY('P', 'E', 0)},
        [WIFI_COUNTRY_PHILIPPINES] = {"PH", CYW43_COUNTRY('P', 'H', 0)},
        [WIFI_COUNTRY_POLAND] = {"PL", CYW43_COUNTRY('P', 'L', 0)},
        [WIFI_COUNTRY_PORTUGAL] = {"PT", CYW43_COUNTRY('P', 'T', 0)},
        [WIFI_COUNTRY_SINGAPORE] = {"SG", CYW43_COUNTRY('S', 'G', 0)},
        [WIFI_COUNTRY_SLOVAKIA] = {"SK", CYW43_COUNTRY('S', 'K', 0)},
        [WIFI_COUNTRY_SLOVENIA] = {"SI", CYW43_COUNTRY('S', 'I', 0)},
        [WIFI_COUNTRY_SOUTH_AFRICA] = {"ZA", CYW43_COUNTRY('Z', 'A', 0)},
        [WIFI_COUNTRY_SOUTH_KOREA] = {"KR", CYW43_COUNTRY('K', 'R', 0)},
        [WIFI_COUNTRY_SPAIN] = {"ES", CYW43_COUNTRY('E', 'S', 0)},
        [WIFI_COUNTRY_SWEDEN] = {"SE", CYW43_COUNTRY('S', 'E', 0)},
        [WIFI_COUNTRY_SWITZERLAND] = {"CH", CYW43_COUNTRY('C', 'H', 0)},
        [WIFI_COUNTRY_TAIWAN] = {"TW", CYW43_COUNTRY('T', 'W', 0)},
        [WIFI_COUNTRY_THAILAND] = {"TH", CYW43_COUNTRY('T', 'H', 0)},
        [WIFI_COUNTRY_TURKEY] = {"TR", CYW43_COUNTRY('T', 'R', 0)},
        [WIFI_COUNTRY_UK] = {"GB", CYW43_COUNTRY('G', 'B', 0)},
        [WIFI_COUNTRY_USA] = {"US", CYW43_COUNTRY('U', 'S', 0)},
    };

    uint32_t wifi_country_to_cyw43(unsigned char idx)
    {
        if (idx >= WIFI_COUNTRY_COUNT)
            return CYW43_COUNTRY('X', 'X', 0);
        return wifi_country_table[idx].cyw43;
    }

    const char *wifi_country_to_string(unsigned char idx)
    {
        if (idx >= WIFI_COUNTRY_COUNT)
            return "XX";
        return wifi_country_table[idx].iso;
    }

    int wifi_country_from_string(const char *iso)
    {
        if (iso == NULL || iso[0] == 0)
            return WIFI_COUNTRY_WORLDWIDE;
        if (strcasecmp(iso, "WW") == 0)
            return WIFI_COUNTRY_WORLDWIDE;
        for (int i = 0; i < WIFI_COUNTRY_COUNT; i++)
        {
            if (strcasecmp(iso, wifi_country_table[i].iso) == 0)
                return i;
        }
        return -1;
    }

    void WebConnect(void)
    {
        if (*Option.SSID)
        {
            if (*Option.ipaddress)
            {
                cyw43_arch_enable_sta_mode();
                dhcp_stop(cyw43_state.netif);
                ip4_addr_t ipaddr, gateway, mask;
                ip4addr_aton(Option.ipaddress, &ipaddr);
                ip4addr_aton(Option.gateway, &gateway);
                ip4addr_aton(Option.mask, &mask);
                netif_set_addr(cyw43_state.netif, &ipaddr, &mask, &gateway);
            }
            else
                cyw43_arch_enable_sta_mode();
            if (*Option.hostname)
            {
                MMPrintString(Option.hostname);
                netif_set_hostname(cyw43_state.netif, Option.hostname);
            }
            cyw43_wifi_pm(&cyw43_state, CYW43_NO_POWERSAVE_MODE);
            MMPrintString(" connecting to WiFi...\r\n");
            int connect_result = cyw43_arch_wifi_connect_timeout_ms((char *)Option.SSID, (char *)(*Option.PASSWORD ? Option.PASSWORD : NULL), (*Option.PASSWORD ? CYW43_AUTH_WPA2_MIXED_PSK : CYW43_AUTH_OPEN), 30000);
            if (connect_result)
            {
                MMPrintString("failed to connect.\r\n");
                WIFIconnected = 0;
                if (connect_result == PICO_ERROR_BADAUTH)
                    LastWifiErr = CYW43_LINK_BADAUTH;
                else
                {
                    int live = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
                    LastWifiErr = (live < 0) ? live : CYW43_LINK_FAIL;
                }
            }
            else
            {
                char buff[STRINGSIZE] = {0};
                sprintf(buff, "Connected %s\r\n", ip4addr_ntoa(netif_ip4_addr(netif_list)));
                MMPrintString(buff);
                WIFIconnected = 1;
                LastWifiErr = 0;
                open_tcp_server();
                if (!Option.disabletftp)
                    cmd_tftp_server_init();
                if (Option.UDP_PORT)
                    open_udp_server();
            }
        }
        else
        {
            cyw43_arch_enable_sta_mode();
            cyw43_wifi_pm(&cyw43_state, CYW43_NO_POWERSAVE_MODE);
        }
        cyw43_wifi_pm(&cyw43_state, CYW43_NO_POWERSAVE_MODE);
    }

    void __not_in_flash_func(ProcessWeb)(int mode)
    {
        static uint64_t flushtimer = 0;
        static uint64_t lastusec = 0;
        static int testcount = 0;
        static int lastonoff = 0;
        static uint64_t lastheartmsec = 0;
        uint64_t timenow = time_us_64();
        if (!WIFIconnected && startupcomplete)
            goto flashonly;
        TCP_SERVER_T *state = (TCP_SERVER_T *)TCPstate;
        if (!state)
            return;
        int t = 0;
        for (int i = 0; i < MaxPcb; i++)
        {
            if (state->client_pcb[i] == NULL)
            {
                t++;
            }
            else if (state->client_pcb[i] == (struct tcp_pcb *)44)
            {
                if (timenow - state->pcbopentime[i] > 1000 * (uint32_t)Option.ServerResponceTime + 20000000 && !state->keepalive[i])
                {
                    state->client_pcb[i] = NULL;
                    //                    printf("PCB %d should be closed by now\r\n", i);
                }
            }
            else
            {
                if (timenow - state->pcbopentime[i] > 1000 * (uint32_t)Option.ServerResponceTime && !state->keepalive[i])
                {
                    //                    printf("Warning PCB %d still open\r\n", i);
                    if (state->buffer_recv[i])
                    {
                        tcp_server_close(state, i);
                        error("No response to request from connection no. %", i + 1);
                        //                            printf("Warning: No response to request from connection no. %d\r\n",i+1);
                    }
                    tcp_server_close(state, i);
                    state->client_pcb[i] = (struct tcp_pcb *)44;
                }
            }
        }
        if (testcount == 0 || timenow > lastusec)
        {
            lastusec = timenow + 1000;
            testcount = 0;
            if (startupcomplete)
                cyw43_arch_poll();
        }
        web_async_check_error();
        testcount++;
        if (testcount == 100)
            testcount = 0;
        if (!mode)
            return;
        if (state->telnet_pcb_no != 99)
        {
            if (timenow > flushtimer)
            {
                TelnetPutC(0, -1);
                flushtimer = timenow + 5000;
            }
        }
    flashonly:;
        if (Option.NoHeartbeat)
        {
            if (lastonoff != 2)
            {
                if (startupcomplete)
                {
                    if (cyw43_arch_gpio_get(CYW43_WL_GPIO_LED_PIN))
                        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
                    lastonoff = 2;
                }
            }
        }
        else
        {
            if (lastonoff == 2)
                lastonoff = 0;
            if (timenow - lastheartmsec > (WIFIconnected ? 500000 : 1000000) && startupcomplete)
            {
                lastheartmsec = timenow;
                if (lastonoff)
                    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
                else
                    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
                lastonoff ^= 1;
            }
        }
    }


/* ---- WEB command handler + WiFi scan (cmd_web), moved from misc/Custom.c ---- */
/* ---- OPTION WIFI parser (setwifi), moved from core/MM_Misc.c ---- */
void setwifi(unsigned char *tp)
{
    getcsargs(&tp, 13);
    if (!(argc == 3 || argc == 5 || argc == 7 || argc == 11 || argc == 13))
        SyntaxError();
    ;
    if (CurrentLinePtr)
        StandardError(10);
    char *ssid = GetTempStrMemory();
    char *password = GetTempStrMemory();
    char *hostname = GetTempStrMemory();
    char *ipaddress = GetTempStrMemory();
    char *mask = GetTempStrMemory();
    char *gateway = GetTempStrMemory();
    strcpy(ssid, (char *)getCstring(argv[0]));
    strcpy(password, (char *)getCstring(argv[2]));
    if (strlen(ssid) > MAXKEYLEN - 1)
        error("SSID too long, max 63 chars");
    if (strlen(password) > MAXKEYLEN - 1)
        error("Password too long, max 63 chars");
    if (argc == 11 || argc == 13)
    {
        strcpy(ipaddress, (char *)getCstring(argv[6]));
        strcpy(mask, (char *)getCstring(argv[8]));
        strcpy(gateway, (char *)getCstring(argv[10]));
        ip4_addr_t ipaddr;
        if (!ip4addr_aton(ipaddress, &ipaddr))
            error("Invalid IP address");
        if (!ip4addr_aton(mask, &ipaddr))
            error("Invalid mask address");
        if (!ip4addr_aton(gateway, &ipaddr))
            error("Invalid gateway address");
    }
    if (argc >= 5 && *argv[4])
    {
        strcpy(hostname, (char *)getCstring(argv[4]));
        if (strlen(hostname) > 31)
            error("Hostname too long, max 31 chars");
    }
    else
    {
        strcpy(hostname, "PICO");
        strcat(hostname, id_out);
    }
    int country_idx = -1;
    if (argc == 7 || argc == 13)
    {
        const char *country_str = (const char *)getCstring(argv[argc == 7 ? 6 : 12]);
        country_idx = wifi_country_from_string(country_str);
        if (country_idx < 0)
            error("Invalid WiFi country code");
    }
    strcpy((char *)Option.SSID, ssid);
    strcpy((char *)Option.PASSWORD, password);
    if (argc == 11 || argc == 13)
    {
        strcpy(Option.ipaddress, ipaddress);
        strcpy(Option.mask, mask);
        strcpy(Option.gateway, gateway);
    }
    else
    {
        memset(Option.ipaddress, 0, 16);
        memset(Option.mask, 0, 16);
        memset(Option.gateway, 0, 16);
    }
    strcpy(Option.hostname, hostname);
    if (country_idx >= 0)
        Option.wifi_country_code = (unsigned char)country_idx;
    SaveOptions();
}

char *scan_dest = NULL;
volatile char *scan_dups = NULL;
volatile int scan_size;
static int scan_result(void *env, const cyw43_ev_scan_result_t *result)
{
        if (result)
        {
                Timer4 = 5000;
                char buff[STRINGSIZE] = {0};
                //        int found=0;
                if (scan_dups == NULL)
                        return 0;
                for (int i = 0; i < scan_dups[32 * 100]; i++)
                {
                        if (strcmp((char *)result->ssid, (char *)&scan_dups[32 * i]) == 0)
                                return 0;
                }
                for (int i = 0; i < 100; i++)
                {
                        if (scan_dups[32 * i] == 0)
                        {
                                strcpy((char *)&scan_dups[32 * i], (char *)result->ssid);
                                scan_dups[32 * 100]++;
                                break;
                        }
                }
                sprintf(buff, "ssid: %-32s rssi: %4d chan: %3d mac: %02x:%02x:%02x:%02x:%02x:%02x sec: %u\r\n",
                        result->ssid, result->rssi, result->channel,
                        result->bssid[0], result->bssid[1], result->bssid[2], result->bssid[3], result->bssid[4], result->bssid[5],
                        result->auth_mode);
                if (scan_dest != NULL)
                {
                        if (strlen((char *)&scan_dest[8]) + strlen(buff) > (size_t)scan_size)
                        {
                                FreeMemorySafe((void **)&scan_dups);
                                scan_dest = NULL;
                                web_async_set_error("Array too small");
                                return 0;
                        }
                        if (scan_dest[8] == 0)
                                strcpy(&scan_dest[8], buff);
                        else
                                strcat(&scan_dest[8], buff);
                }
                else
                        MMPrintString(buff);
        }
        return 0;
}
/*  @endcond */
void cmd_web(void)
{
        unsigned char *tp;
        tp = checkstring(cmdline, (unsigned char *)"CONNECT");
        if (tp)
        {
                if (*tp)
                {
                        setwifi(tp);
                        WebConnect();
                }
                else
                {
                        if (cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA) < 0)
                        {
                                WebConnect();
                        }
                }
                return;
        }
        tp = checkstring(cmdline, (unsigned char *)"SCAN");
        if (tp)
        {
                void *ptr1 = NULL;
                if (*tp)
                {
                        ptr1 = findvar(tp, V_FIND | V_EMPTY_OK);
                        CHECK_STRUCT_MEMBER_ARRAY(); // Struct member arrays not supported here
                        if (g_vartbl[g_VarIndex].type & T_INT)
                        {
                                if (g_vartbl[g_VarIndex].dims[1] != 0)
                                        StandardError(6);
                                if (g_vartbl[g_VarIndex].dims[0] <= 0)
                                { // Not an array
                                        StandardError(35);
                                }
                                scan_size = (g_vartbl[g_VarIndex].dims[0] - g_OptionBase) * 8;
                                scan_dest = (char *)ptr1;
                                scan_dest[8] = 0;
                        }
                        else
                                StandardError(35);
                }
                scan_dups = GetMemory(32 * 100 + 1);
                cyw43_wifi_scan_options_t scan_options = {0};
                int err = cyw43_wifi_scan(&cyw43_state, &scan_options, NULL, scan_result);
                if (err == 0)
                {
                        MMPrintString("\nPerforming wifi scan\r\n");
                }
                else
                {
                        char buff[STRINGSIZE] = {0};
                        sprintf(buff, "Failed to start scan: %d\r\n", err);
                        MMPrintString(buff);
                }
                Timer4 = 500;
                while (Timer4)
                        if (startupcomplete)
                                ProcessWeb(0);
                if (scan_dest)
                {
                        uint64_t *p = (uint64_t *)scan_dest;
                        *p = strlen(&scan_dest[8]);
                }
                scan_dest = NULL;
                FreeMemorySafe((void **)&scan_dups);
                return;
        }
        if (!(WIFIconnected && cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP))
                error("WIFI not connected");
        tp = checkstring(cmdline, (unsigned char *)"NTP");
        if (tp)
        {
                cmd_ntp(tp);
                return;
        }
        tp = checkstring(cmdline, (unsigned char *)"UDP");
        if (tp)
        {
                cmd_udp(tp);
                return;
        }
        if (cmd_mqtt())
                return;
        if (cmd_tcpclient())
                return;
        if (cmd_tcpserver())
                return;
#ifdef PICOMITEWEB_TLS
        if (cmd_tls())
                return;
#endif
        SyntaxError();
        ;
}

#endif // PICOMITEWEB
