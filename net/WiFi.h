/* ============================================================================
 * WiFi.h - interface to the WiFi / lwIP / web runtime (net/WiFi.c)
 *
 * Declares the functions PicoMite.c (and the MM* net command files) call into
 * after the WiFi/web split. Most shared state is already extern in
 * Hardware_Includes.h; only the extras referenced across this boundary go here.
 * ============================================================================ */
#ifndef WIFI_H
#define WIFI_H

#ifdef PICOMITEWEB

/* lwIP poll pump, called from PicoMite.c (CheckAbort / routinechecks). */
void ProcessWeb(int mode);

/* WiFi association + deferred async-error helpers (set from lwIP/cyw43
   callbacks that stay in PicoMite.c, checked from the main thread). */
void WebConnect(void);
void web_async_set_error(const char *msg);
void web_async_check_error(void);
int wifi_country_from_string(const char *iso);

/* OPTION WIFI parser — sets the WiFi Option fields then triggers a soft reset.
   Called from core/MM_Misc.c (the OPTION command), defined in net/WiFi.c. */
void setwifi(unsigned char *tp);

#endif // PICOMITEWEB
#endif // WIFI_H
