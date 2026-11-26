/*
 * BTSerial.c - a Bluetooth serial device as COM7, for the BT host builds
 * (PICOMITEBTH, PICOMITEHDMIBTH): a Bluetooth Classic device with the Serial
 * Port Profile (RFCOMM) - a Bluetooth GPS, say - or a BLE device with a
 * serial service: the Nordic UART Service (a PicoMiteBT's console, many
 * modules), the HM-10's FFE0 service, or Microchip's Transparent UART.
 * One device at a time.
 *
 * Classic: BLUETOOTH SCAN finds the device and BLUETOOTH CONNECT name$ calls
 * bt_serial_connect() for anything that is neither audio nor a keyboard.
 * That asks the device's SDP server for a Serial Port record (UUID 0x1101,
 * searched by its class: such records may have no browse group) and its
 * RFCOMM channel. A device without one is left to the speaker code. The
 * channel is opened at btstack's default security level 2, so the device
 * is paired - legacy PIN "0000", or Just Works - while CONNECT runs.
 *
 * BLE: BLUETOOTH SCAN also lists BLE advertisers that name one of the
 * serial services (BTAudio.c asks bt_serial_le_service_in_ad()), and CONNECT
 * calls bt_serial_connect_le() for them. The link is made through
 * BTKeyboard.c (bt_keyboard_connect_other: btstack makes one LE connection
 * at a time), then the service and its two characteristics are found and
 * the device's transmit characteristic is told to notify. Bytes for the
 * device are written to its receive characteristic without response. No
 * pairing is asked for; a device that wants it asks, and BTKeyboard.c's
 * Security Manager handler answers.
 *
 * Either way the device is remembered in the bond store (tag 'BTSP').
 *
 * COM7 then behaves as a USB CDC port does (Serial.c): received bytes go to
 * the port's buffer, or to the NMEA decoder with OPEN ... AS GPS; sent
 * bytes are buffered here and go out when btstack can send. The baud rate
 * is accepted and ignored. While COM7 is open, the remembered device is
 * reconnected when it is not connected, and a program sees nothing but a
 * gap in the data: a Classic device is paged at OPEN and after a drop-out,
 * at intervals doubling from 5 s to a minute (each attempt pages for at most
 * 5.12 s); a BLE device is connected when it advertises, LE scanning staying
 * on meanwhile (bt_keyboard_scan_hold). A closed port pages and scans for
 * nothing, and a link left up by CLOSE stays up.
 *
 * Threading: the handlers run in btstack's context inside cyw43_arch_poll()
 * (bt_keyboard_poll) and never print - console messages go through
 * bt_notice(). The interpreter takes the async-context lock around btstack
 * calls. The send buffer is filled by the interpreter and drained by the
 * handlers; both run on core 0's main thread, the handlers only inside the
 * poll, so neither interrupts the other.
 */
#if defined(PICOMITEBTH) || defined(PICOMITEHDMIBTH)

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "configuration.h"
#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"

#include "pico/cyw43_arch.h"
#undef UNUSED
#include "btstack.h"
#include "btstack_tlv.h"

#include "BTSerial.h"
#include "BTAudio.h"
#include "BTKeyboard.h"

#define SP_TLV_TAG 0x42545350u /* 'BTSP': the remembered device */
#define SP_QUERY_TIMEOUT_MS 20000
#define SP_CONNECT_TIMEOUT_MS 30000
#define SP_LE_LINK_TIMEOUT_MS 10000 /* an LE connection not made by then is cancelled */
#define SP_RECONNECT_FIRST_MS 10
#define SP_RECONNECT_MIN_RETRY_MS 5000
#define SP_RECONNECT_MAX_RETRY_MS 60000
#define SP_TX_SIZE 256

typedef enum
{
    SP_NONE,
    SP_QUERYING,   /* Classic CONNECT: asking for the serial port's RFCOMM channel */
    SP_CONNECTING, /* rfcomm_create_channel(), or the LE link and its GATT set-up */
    SP_CONNECTED,
} sp_state_t;

/* The LE set-up, in order */
typedef enum
{
    SPL_LINK,    /* the LE connection */
    SPL_SERVICE, /* finding the serial service */
    SPL_CHARS,   /* finding its two characteristics */
    SPL_NOTIFY,  /* switching on notifications of the transmit one */
} spl_step_t;

typedef struct
{
    uint8_t addr[6];
    uint8_t channel;   /* Classic: the RFCOMM channel */
    uint8_t le;        /* a BLE device */
    uint8_t addr_type; /* BLE: its address type */
    uint8_t service;   /* BLE: its sp_le_services[] entry */
} sp_remembered_t;
#define SP_REMEMBERED_CLASSIC_SIZE 7 /* what the first version stored: addr and channel */

/* BLE serial services. UUIDs in the usual order (most significant byte
   first), as btstack's GATT client reports them. */
typedef struct
{
    const char *name;
    uint8_t service[16];
    uint8_t rx[16]; /* the device receives: we write it */
    uint8_t tx[16]; /* the device transmits: it notifies */
} sp_le_service_t;

static const sp_le_service_t sp_le_services[] = {
    {"Nordic UART",
     {0x6E, 0x40, 0x00, 0x01, 0xB5, 0xA3, 0xF3, 0x93, 0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E},
     {0x6E, 0x40, 0x00, 0x02, 0xB5, 0xA3, 0xF3, 0x93, 0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E},
     {0x6E, 0x40, 0x00, 0x03, 0xB5, 0xA3, 0xF3, 0x93, 0xE0, 0xA9, 0xE5, 0x0E, 0x24, 0xDC, 0xCA, 0x9E}},
    {"HM-10", /* one characteristic both ways */
     {0x00, 0x00, 0xFF, 0xE0, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB},
     {0x00, 0x00, 0xFF, 0xE1, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB},
     {0x00, 0x00, 0xFF, 0xE1, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB}},
    {"Microchip Transparent UART",
     {0x49, 0x53, 0x53, 0x43, 0xFE, 0x7D, 0x4A, 0xE5, 0x8F, 0xA9, 0x9F, 0xAF, 0xD2, 0x05, 0xE4, 0x55},
     {0x49, 0x53, 0x53, 0x43, 0x88, 0x41, 0x43, 0xF4, 0xA8, 0xD4, 0xEC, 0xBE, 0x34, 0x72, 0x9B, 0xB3},
     {0x49, 0x53, 0x53, 0x43, 0x1E, 0x4D, 0x4B, 0xD9, 0xBA, 0x61, 0x23, 0xC6, 0x47, 0x24, 0x96, 0x16}},
};
#define SP_LE_SERVICES ((int)(sizeof(sp_le_services) / sizeof(sp_le_services[0])))

static volatile sp_state_t sp_state;
static bd_addr_t sp_addr;            /* the device connecting or connected */
static bool sp_le;                   /* ...is a BLE device */
static bool sp_by_connect;           /* ...by BLUETOOTH CONNECT, not a reconnect */
static uint8_t sp_channel;           /* Classic: its RFCOMM server channel */
static uint16_t sp_cid;              /* Classic: the RFCOMM channel */
static uint16_t sp_frame_size;       /* the largest frame (RFCOMM) or write (BLE) it takes */
static volatile uint8_t sp_last_status;
static volatile bool sp_pairing;     /* Classic CONNECT running: pairing allowed with sp_addr */
static volatile bool sp_quiet_close; /* our own disconnect: no message, no reconnect */
static sp_remembered_t sp_remembered;
static bool sp_have_remembered;
static volatile uint32_t sp_received, sp_sent; /* bytes, since connecting (BLUETOOTH STATUS) */

/* BLE */
static uint8_t sp_addr_type;
static uint8_t sp_service; /* sp_le_services[] */
static hci_con_handle_t sp_con = HCI_CON_HANDLE_INVALID;
static spl_step_t sp_step;
static bool sp_link_pending;        /* bt_keyboard_connect_other() made, not yet complete */
static volatile bool sp_no_service; /* the device lacks the service or a characteristic */
static bool sp_found_service;
static uint8_t sp_found_chars; /* bit 0: rx, bit 1: tx */
static gatt_client_service_t sp_gatt_service;
static gatt_client_characteristic_t sp_rx_char, sp_tx_char;
static gatt_client_notification_t sp_listener;
static bool sp_listening;
static bool sp_scan_held;
static btstack_timer_source_t sp_link_timer;

/* While COM7 is open (bt_serial_port_opened), the remembered device is
   reconnected. */
static volatile bool sp_auto;
static btstack_timer_source_t sp_timer;
static uint32_t sp_retry_ms;

/* Bytes for the device: the interpreter moves the head, the handlers the tail. */
static uint8_t sp_tx_buf[SP_TX_SIZE];
static volatile uint16_t sp_tx_head, sp_tx_tail;
static volatile bool sp_send_requested;

static btstack_packet_callback_registration_t sp_hci_registration;

static void sp_rfcomm_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void sp_gatt_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void sp_reconnect_after(uint32_t first_ms);

static inline void sp_lock(void)
{
    async_context_acquire_lock_blocking(cyw43_arch_async_context());
}

static inline void sp_unlock(void)
{
    async_context_release_lock(cyw43_arch_async_context());
}

bool bt_serial_is_remembered(const uint8_t *addr)
{
    return sp_have_remembered && memcmp(addr, sp_remembered.addr, sizeof(bd_addr_t)) == 0;
}

bool bt_serial_remembered_le(const uint8_t *addr, uint8_t *addr_type, uint8_t *service)
{
    if (!bt_serial_is_remembered(addr) || !sp_remembered.le)
        return false;
    *addr_type = sp_remembered.addr_type;
    *service = sp_remembered.service;
    return true;
}

bool bt_serial_owns_pairing(const uint8_t *addr)
{
    return sp_pairing && memcmp(addr, sp_addr, sizeof(bd_addr_t)) == 0;
}

static void sp_remember(void)
{
    sp_remembered_t r;
    memset(&r, 0, sizeof(r));
    memcpy(r.addr, sp_addr, sizeof(bd_addr_t));
    r.le = sp_le;
    if (sp_le)
    {
        r.addr_type = sp_addr_type;
        r.service = sp_service;
    }
    else
        r.channel = sp_channel;
    if (sp_have_remembered && memcmp(&r, &sp_remembered, sizeof(r)) == 0)
        return; /* unchanged - no bond-store write */
    sp_remembered = r;
    sp_have_remembered = true;
    const btstack_tlv_t *tlv;
    void *ctx;
    btstack_tlv_get_instance(&tlv, &ctx);
    if (tlv)
        tlv->store_tag(ctx, SP_TLV_TAG, (const uint8_t *)&sp_remembered, sizeof(sp_remembered));
}

/* LE scanning, for a remembered BLE device COM7 is waiting for. btstack
   context, or the lock held. */
static void sp_update_scan_hold(void)
{
    bool want = sp_auto && sp_have_remembered && sp_remembered.le && sp_state != SP_CONNECTED;
    if (want != sp_scan_held)
    {
        sp_scan_held = want;
        bt_keyboard_scan_hold(BT_SCAN_HOLD_SERIAL, want);
    }
}

/* ============================================================================
 * BLE serial services in advertising data (BLUETOOTH SCAN, via BTAudio.c)
 * ============================================================================
 */
int bt_serial_le_service_in_ad(const uint8_t *data, uint8_t len)
{
    ad_context_t ctx;
    for (ad_iterator_init(&ctx, len, data); ad_iterator_has_more(&ctx); ad_iterator_next(&ctx))
    {
        uint8_t type = ad_iterator_get_data_type(&ctx);
        uint8_t n = ad_iterator_get_data_len(&ctx);
        const uint8_t *d = ad_iterator_get_data(&ctx);
        bool list128 = type == BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS ||
                       type == BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS;
        bool list16 = type == BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS ||
                      type == BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS;
        if (!list128 && !list16)
            continue;
        for (int s = 0; s < SP_LE_SERVICES; s++)
        {
            const uint8_t *u = sp_le_services[s].service;
            if (list128)
            {
                for (int i = 0; i + 16 <= n; i += 16)
                {
                    uint8_t be[16]; /* advertising data is least significant byte first */
                    reverse_128(d + i, be);
                    if (memcmp(be, u, 16) == 0)
                        return s;
                }
            }
            else if (uuid_has_bluetooth_prefix(u))
            {
                for (int i = 0; i + 2 <= n; i += 2)
                    if (little_endian_read_16(d, i) == big_endian_read_16(u, 2))
                        return s;
            }
        }
    }
    return -1;
}

/* ============================================================================
 * Sending (btstack context)
 * ============================================================================
 */
/* Ask for the event that says the device can take more (RFCOMM or GATT).
   btstack may deliver it inside this call, and the handler clears the flag,
   so the flag is set first. */
static void sp_request_send(void)
{
    sp_send_requested = true;
    uint8_t status = sp_le ? gatt_client_request_can_write_without_response_event(&sp_gatt_handler, sp_con)
                           : rfcomm_request_can_send_now_event(sp_cid);
    if (status != ERROR_CODE_SUCCESS)
        sp_send_requested = false;
}

static void sp_send(void)
{
    if (sp_state != SP_CONNECTED)
        return;
    uint16_t head = sp_tx_head, tail = sp_tx_tail;
    if (head == tail)
        return;
    uint16_t n = head > tail ? head - tail : SP_TX_SIZE - tail; /* the contiguous part */
    if (n > sp_frame_size)
        n = sp_frame_size;
    uint8_t status = sp_le ? gatt_client_write_value_of_characteristic_without_response(
                                 sp_con, sp_rx_char.value_handle, n, &sp_tx_buf[tail])
                           : rfcomm_send(sp_cid, &sp_tx_buf[tail], n);
    if (status == ERROR_CODE_SUCCESS)
    {
        sp_tx_tail = (uint16_t)((tail + n) % SP_TX_SIZE);
        sp_sent += n;
    }
    if (sp_tx_head != sp_tx_tail)
        sp_request_send();
}

/* ============================================================================
 * Connected and closed - both kinds of device (btstack context)
 * ============================================================================
 */
static void sp_connected(void)
{
    sp_pairing = false;
    sp_received = sp_sent = 0;
    sp_tx_tail = sp_tx_head; /* nothing older than the connection */
    sp_send_requested = false;
    sp_state = SP_CONNECTED;
    if (sp_by_connect)
        sp_remember(); /* here, not in CONNECT: Ctrl-C may have left it */
    else if (!CurrentLinePtr)
        bt_notice("Bluetooth serial device connected (COM7)\r\n> ");
    sp_update_scan_hold();
}

/* The link has gone, or a connection attempt failed with status. */
static void sp_closed(uint8_t status)
{
    bool was_connected = sp_state == SP_CONNECTED;
    if (!was_connected)
        sp_last_status = status;
    sp_state = SP_NONE;
    sp_pairing = false;
    sp_send_requested = false;
    sp_tx_tail = sp_tx_head;
    if (sp_listening)
    {
        gatt_client_stop_listening_for_characteristic_value_updates(&sp_listener);
        sp_listening = false;
    }
    sp_con = HCI_CON_HANDLE_INVALID;
    if (was_connected && !sp_quiet_close)
    {
        if (!CurrentLinePtr)
            bt_notice("Bluetooth serial device disconnected (COM7)\r\n> ");
        if (!sp_le)
            sp_reconnect_after(SP_RECONNECT_MIN_RETRY_MS); /* switched off, or out of range */
    }
    sp_quiet_close = false;
    sp_update_scan_hold(); /* a BLE device is reconnected when it advertises */
}

/* ============================================================================
 * The BLE link (btstack context, or the lock held)
 * ============================================================================
 */
static void sp_link_timeout(btstack_timer_source_t *ts)
{
    (void)ts;
    if (sp_link_pending)
        gap_connect_cancel(); /* the connection-complete event that follows ends it */
}

/* Starts connecting a BLE device. ERROR_CODE_COMMAND_DISALLOWED: another LE
   connection is being made - try again. */
static uint8_t sp_le_start(const uint8_t *addr, uint8_t addr_type, uint8_t service, bool by_connect)
{
    if (sp_state != SP_NONE)
        return ERROR_CODE_COMMAND_DISALLOWED;
    uint8_t status = bt_keyboard_connect_other(addr, addr_type);
    if (status != ERROR_CODE_SUCCESS)
        return status;
    memcpy(sp_addr, addr, sizeof(bd_addr_t));
    sp_le = true;
    sp_by_connect = by_connect;
    sp_addr_type = addr_type;
    sp_service = service;
    sp_last_status = 0;
    sp_no_service = false;
    sp_link_pending = true;
    sp_step = SPL_LINK;
    sp_state = SP_CONNECTING;
    btstack_run_loop_remove_timer(&sp_link_timer);
    btstack_run_loop_set_timer_handler(&sp_link_timer, sp_link_timeout);
    btstack_run_loop_set_timer(&sp_link_timer, SP_LE_LINK_TIMEOUT_MS);
    btstack_run_loop_add_timer(&sp_link_timer);
    return ERROR_CODE_SUCCESS;
}

/* A step of the GATT set-up failed: drop the link. */
static void sp_le_fail(uint8_t status)
{
    sp_last_status = status;
    sp_quiet_close = true;
    gap_disconnect(sp_con); /* sp_closed() follows the disconnection */
}

static bool sp_uuid_is(const gatt_client_characteristic_t *c, const uint8_t *uuid)
{
    return memcmp(c->uuid128, uuid, 16) == 0;
}

static void sp_gatt_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    (void)channel;
    (void)size;
    if (packet_type != HCI_EVENT_PACKET)
        return;
    const sp_le_service_t *svc = &sp_le_services[sp_service];

    switch (hci_event_packet_get_type(packet))
    {
    case GATT_EVENT_NOTIFICATION:
        if (gatt_event_notification_get_handle(packet) == sp_con && sp_state == SP_CONNECTED &&
            gatt_event_notification_get_value_handle(packet) == sp_tx_char.value_handle)
        {
            uint16_t n = gatt_event_notification_get_value_length(packet);
            sp_received += n;
            SerialBTReceive(gatt_event_notification_get_value(packet), n);
        }
        break;

    case GATT_EVENT_CAN_WRITE_WITHOUT_RESPONSE:
        if (gatt_event_can_write_without_response_get_handle(packet) != sp_con)
            break;
        sp_send_requested = false;
        sp_send();
        break;

    case GATT_EVENT_SERVICE_QUERY_RESULT:
        if (sp_state == SP_CONNECTING && sp_step == SPL_SERVICE)
        {
            gatt_event_service_query_result_get_service(packet, &sp_gatt_service);
            sp_found_service = true;
        }
        break;

    case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
        if (sp_state == SP_CONNECTING && sp_step == SPL_CHARS)
        {
            gatt_client_characteristic_t c;
            gatt_event_characteristic_query_result_get_characteristic(packet, &c);
            if (sp_uuid_is(&c, svc->rx))
            {
                sp_rx_char = c;
                sp_found_chars |= 1;
            }
            if (sp_uuid_is(&c, svc->tx))
            {
                sp_tx_char = c;
                sp_found_chars |= 2;
            }
        }
        break;

    case GATT_EVENT_QUERY_COMPLETE:
    {
        if (gatt_event_query_complete_get_handle(packet) != sp_con || sp_state != SP_CONNECTING)
            break;
        uint8_t att = gatt_event_query_complete_get_att_status(packet);
        if (att != ATT_ERROR_SUCCESS)
        {
            sp_le_fail(att);
            break;
        }
        switch (sp_step)
        {
        case SPL_SERVICE:
            if (!sp_found_service)
            {
                sp_no_service = true;
                sp_le_fail(ATT_ERROR_ATTRIBUTE_NOT_FOUND);
                break;
            }
            sp_found_chars = 0;
            sp_step = SPL_CHARS;
            if (gatt_client_discover_characteristics_for_service(&sp_gatt_handler, sp_con, &sp_gatt_service) !=
                ERROR_CODE_SUCCESS)
                sp_le_fail(ERROR_CODE_UNSPECIFIED_ERROR);
            break;
        case SPL_CHARS:
            if (sp_found_chars != 3)
            {
                sp_no_service = true;
                sp_le_fail(ATT_ERROR_ATTRIBUTE_NOT_FOUND);
                break;
            }
            sp_step = SPL_NOTIFY;
            if (gatt_client_write_client_characteristic_configuration(
                    &sp_gatt_handler, sp_con, &sp_tx_char, GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION) !=
                ERROR_CODE_SUCCESS)
                sp_le_fail(ERROR_CODE_UNSPECIFIED_ERROR);
            break;
        case SPL_NOTIFY:
        {
            gatt_client_listen_for_characteristic_value_updates(&sp_listener, &sp_gatt_handler, sp_con, &sp_tx_char);
            sp_listening = true;
            uint16_t mtu = ATT_DEFAULT_MTU;
            gatt_client_get_mtu(sp_con, &mtu);
            sp_frame_size = (uint16_t)(mtu - 3); /* a write's payload */
            sp_connected();
            break;
        }
        default:
            break;
        }
        break;
    }

    default:
        break;
    }
}

/* ============================================================================
 * Reconnecting while COM7 is open (btstack context)
 * ============================================================================
 */
/* A Classic device is paged by this timer; a BLE device is connected when
   it advertises (sp_hci_handler). */
static void sp_reconnect_timeout(btstack_timer_source_t *ts)
{
    if (!sp_auto)
        return;
    if (sp_have_remembered && !sp_remembered.le && sp_state == SP_NONE && !bt_audio_inquiry_busy())
    {
        uint16_t cid;
        memcpy(sp_addr, sp_remembered.addr, sizeof(bd_addr_t));
        sp_channel = sp_remembered.channel;
        if (rfcomm_create_channel(&sp_rfcomm_handler, sp_addr, sp_channel, &cid) == ERROR_CODE_SUCCESS)
        {
            sp_cid = cid;
            sp_le = false;
            sp_by_connect = false;
            sp_last_status = 0;
            sp_state = SP_CONNECTING;
        }
    }
    btstack_run_loop_set_timer(ts, sp_retry_ms);
    btstack_run_loop_add_timer(ts);
    sp_retry_ms *= 2;
    if (sp_retry_ms > SP_RECONNECT_MAX_RETRY_MS)
        sp_retry_ms = SP_RECONNECT_MAX_RETRY_MS;
}

static void sp_reconnect_after(uint32_t first_ms)
{
    if (!sp_auto)
        return;
    sp_retry_ms = SP_RECONNECT_MIN_RETRY_MS;
    btstack_run_loop_remove_timer(&sp_timer);
    btstack_run_loop_set_timer_handler(&sp_timer, sp_reconnect_timeout);
    btstack_run_loop_set_timer(&sp_timer, first_ms);
    btstack_run_loop_add_timer(&sp_timer);
}

/* ============================================================================
 * btstack handlers
 * ============================================================================
 */
static void sp_rfcomm_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    if (packet_type == RFCOMM_DATA_PACKET)
    {
        if (!sp_le && channel == sp_cid && sp_state == SP_CONNECTED)
        {
            sp_received += size;
            SerialBTReceive(packet, size);
        }
        return;
    }
    if (packet_type != HCI_EVENT_PACKET || sp_le)
        return;

    switch (hci_event_packet_get_type(packet))
    {
    case RFCOMM_EVENT_CHANNEL_OPENED:
    {
        if (rfcomm_event_channel_opened_get_rfcomm_cid(packet) != sp_cid || sp_state != SP_CONNECTING)
            break;
        uint8_t status = rfcomm_event_channel_opened_get_status(packet);
        if (status != ERROR_CODE_SUCCESS)
        {
            sp_closed(status); /* a reconnect: the timer tries again */
            break;
        }
        sp_frame_size = rfcomm_event_channel_opened_get_max_frame_size(packet);
        sp_connected();
        break;
    }

    case RFCOMM_EVENT_CHANNEL_CLOSED:
        if (rfcomm_event_channel_closed_get_rfcomm_cid(packet) == sp_cid)
            sp_closed(0);
        break;

    case RFCOMM_EVENT_CAN_SEND_NOW:
        if (rfcomm_event_can_send_now_get_rfcomm_cid(packet) != sp_cid)
            break;
        sp_send_requested = false;
        sp_send();
        break;

    default:
        break;
    }
}

/* Classic CONNECT's SDP query: the first Serial Port record's RFCOMM channel. */
static void sp_sdp_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    (void)channel;
    (void)size;
    if (packet_type != HCI_EVENT_PACKET)
        return;
    switch (hci_event_packet_get_type(packet))
    {
    case SDP_EVENT_QUERY_RFCOMM_SERVICE:
        if (sp_channel == 0)
            sp_channel = sdp_event_query_rfcomm_service_get_rfcomm_channel(packet);
        break;
    case SDP_EVENT_QUERY_COMPLETE:
        sp_last_status = sdp_event_query_complete_get_status(packet);
        if (sp_state == SP_QUERYING)
            sp_state = SP_NONE;
        break;
    default:
        break;
    }
}

/* Classic pairing requests for the device CONNECT is pairing (BTAudio.c's
   handler leaves these alone: bt_serial_owns_pairing), and the BLE link's
   own events. */
static void sp_hci_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    (void)channel;
    (void)size;
    if (packet_type != HCI_EVENT_PACKET)
        return;
    bd_addr_t addr;

    switch (hci_event_packet_get_type(packet))
    {
    case BTSTACK_EVENT_STATE:
        if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING)
        {
            const btstack_tlv_t *tlv;
            void *ctx;
            btstack_tlv_get_instance(&tlv, &ctx);
            if (tlv)
            {
                memset(&sp_remembered, 0, sizeof(sp_remembered));
                int n = tlv->get_tag(ctx, SP_TLV_TAG, (uint8_t *)&sp_remembered, sizeof(sp_remembered));
                if (n == SP_REMEMBERED_CLASSIC_SIZE)
                    sp_remembered.le = 0;
                sp_have_remembered = n == (int)sizeof(sp_remembered) || n == SP_REMEMBERED_CLASSIC_SIZE;
                if (sp_remembered.le && sp_remembered.service >= SP_LE_SERVICES)
                    sp_have_remembered = false;
            }
        }
        break;

    case HCI_EVENT_PIN_CODE_REQUEST:
        /* Legacy pairing: the PIN given to BLUETOOTH CONNECT, or "0000",
           which serial devices, GPS receivers among them, mostly take. */
        hci_event_pin_code_request_get_bd_addr(packet, addr);
        if (bt_serial_owns_pairing(addr))
            gap_pin_code_response(addr, bt_connect_pin());
        break;

    case HCI_EVENT_USER_CONFIRMATION_REQUEST:
        /* Secure Simple Pairing, Just Works. */
        hci_event_user_confirmation_request_get_bd_addr(packet, addr);
        if (bt_serial_owns_pairing(addr))
            gap_ssp_confirmation_response(addr);
        break;

    case GAP_EVENT_ADVERTISING_REPORT:
        /* The remembered BLE device advertises while COM7 waits for it. */
        if (sp_auto && sp_state == SP_NONE && sp_have_remembered && sp_remembered.le)
        {
            gap_event_advertising_report_get_address(packet, addr);
            if (memcmp(addr, sp_remembered.addr, sizeof(bd_addr_t)) == 0)
                sp_le_start(addr, gap_event_advertising_report_get_address_type(packet), sp_remembered.service,
                            false); /* refused while another LE connection is made: its next advertisement */
        }
        break;

    case HCI_EVENT_LE_META:
        if (hci_event_le_meta_get_subevent_code(packet) != HCI_SUBEVENT_LE_CONNECTION_COMPLETE || !sp_link_pending)
            break;
        /* bt_keyboard_connect_other() makes one connection at a time: this is ours */
        sp_link_pending = false;
        btstack_run_loop_remove_timer(&sp_link_timer);
        if (hci_subevent_le_connection_complete_get_status(packet) != ERROR_CODE_SUCCESS)
        {
            sp_closed(hci_subevent_le_connection_complete_get_status(packet));
            break;
        }
        sp_con = hci_subevent_le_connection_complete_get_connection_handle(packet);
        sp_found_service = false;
        sp_step = SPL_SERVICE;
        {
            /* The search compares the attribute's value byte for byte, and a
               16-bit UUID is stored as 2 bytes: the HM-10's FFE0 searched for
               in its 128-bit form was never found. */
            const uint8_t *u = sp_le_services[sp_service].service;
            uint8_t status = uuid_has_bluetooth_prefix(u)
                                 ? gatt_client_discover_primary_services_by_uuid16(&sp_gatt_handler, sp_con,
                                                                                   big_endian_read_16(u, 2))
                                 : gatt_client_discover_primary_services_by_uuid128(&sp_gatt_handler, sp_con, u);
            if (status != ERROR_CODE_SUCCESS)
                sp_le_fail(ERROR_CODE_UNSPECIFIED_ERROR);
        }
        break;

    case HCI_EVENT_DISCONNECTION_COMPLETE:
        if (sp_le && sp_con != HCI_CON_HANDLE_INVALID &&
            hci_event_disconnection_complete_get_connection_handle(packet) == sp_con)
            sp_closed(sp_last_status ? sp_last_status : hci_event_disconnection_complete_get_reason(packet));
        break;

    default:
        break;
    }
}

void bt_serial_init(void)
{
    sp_state = SP_NONE;
    rfcomm_init();
    sp_hci_registration.callback = &sp_hci_handler;
    hci_add_event_handler(&sp_hci_registration);
}

/* ============================================================================
 * BLUETOOTH CONNECT, DISCONNECT SERIAL, STATUS, FORGET and MM.INFO
 * ============================================================================
 */
/* Wait in the interpreter while the state stays as it is, keeping Ctrl-C
   and the background services alive. Returns false on timeout. */
static bool sp_wait_while(sp_state_t state, uint32_t timeout_ms)
{
    uint64_t end = time_us_64() + (uint64_t)timeout_ms * 1000u;
    while (sp_state == state)
    {
        { CheckAbort(); NetYield(); }
        if (time_us_64() > end)
            return false;
    }
    return true;
}

/* error() with a btstack status code ('&' is error()'s float placeholder,
   so the hex is formatted here). */
static void sp_error_status(const char *what, uint8_t status)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%s (Bluetooth error 0x%02X)", what, status);
    error("$", buf);
}

bool bt_serial_connect(const uint8_t *addr)
{
    if (sp_state != SP_NONE)
        error(sp_state == SP_CONNECTED ? "Serial device already connected" : "Serial device already connecting");

    /* Its serial port, if it has one. The SDP client serves one query at a
       time; a keyboard or speaker connecting may hold it briefly. */
    uint64_t end = time_us_64() + 5000000u;
    uint8_t status;
    for (;;)
    {
        sp_lock();
        status = SDP_QUERY_BUSY;
        if (sdp_client_ready())
        {
            memcpy(sp_addr, addr, sizeof(bd_addr_t));
            sp_le = false;
            sp_channel = 0;
            sp_last_status = 0;
            status = sdp_client_query_rfcomm_channel_and_name_for_service_class_uuid(
                &sp_sdp_handler, sp_addr, BLUETOOTH_SERVICE_CLASS_SERIAL_PORT);
            if (status == ERROR_CODE_SUCCESS)
                sp_state = SP_QUERYING;
        }
        sp_unlock();
        if (status != SDP_QUERY_BUSY || time_us_64() > end)
            break;
        CheckAbort();
    }
    if (status != ERROR_CODE_SUCCESS)
        sp_error_status("Connect failed", status);
    if (!sp_wait_while(SP_QUERYING, SP_QUERY_TIMEOUT_MS))
    {
        sp_state = SP_NONE;
        error("Device did not answer");
    }
    if (sp_last_status != ERROR_CODE_SUCCESS)
        sp_error_status("Connection failed", sp_last_status);
    if (sp_channel == 0)
        return false; /* no serial port */

    /* Open it; pairing is allowed meanwhile. */
    uint16_t cid;
    sp_lock();
    sp_last_status = 0;
    sp_by_connect = true;
    sp_pairing = true;
    status = rfcomm_create_channel(&sp_rfcomm_handler, sp_addr, sp_channel, &cid);
    if (status == ERROR_CODE_SUCCESS)
    {
        sp_cid = cid;
        sp_state = SP_CONNECTING;
    }
    else
        sp_pairing = false;
    sp_unlock();
    if (status != ERROR_CODE_SUCCESS)
        sp_error_status("Connect failed", status);

    bool answered = sp_wait_while(SP_CONNECTING, SP_CONNECT_TIMEOUT_MS);
    sp_pairing = false;
    if (!answered)
    {
        sp_lock();
        sp_quiet_close = true;
        rfcomm_disconnect(sp_cid);
        sp_unlock();
        sp_state = SP_NONE;
        error("Serial device did not answer");
    }
    if (sp_state != SP_CONNECTED)
        sp_error_status("Connection failed", sp_last_status);

    char buf[40];
    MMPrintString("Connected to serial device ");
    MMPrintString((char *)bd_addr_to_str(sp_addr));
    snprintf(buf, sizeof(buf), " (RFCOMM channel %u) as COM7", (unsigned)sp_channel);
    MMPrintString(buf);
    PRet();
    return true;
}

void bt_serial_connect_le(const uint8_t *addr, uint8_t addr_type, uint8_t service)
{
    if (sp_state != SP_NONE)
        error(sp_state == SP_CONNECTED ? "Serial device already connected" : "Serial device already connecting");
    if (service >= SP_LE_SERVICES)
        error("Not a BLE serial device");

    /* btstack makes one LE connection at a time: the keyboard may be making one. */
    uint64_t end = time_us_64() + 5000000u;
    uint8_t status;
    for (;;)
    {
        sp_lock();
        status = sp_le_start(addr, addr_type, service, true);
        sp_unlock();
        if (status != ERROR_CODE_COMMAND_DISALLOWED || time_us_64() > end)
            break;
        CheckAbort();
    }
    if (status != ERROR_CODE_SUCCESS)
        sp_error_status("Connect failed", status);

    if (!sp_wait_while(SP_CONNECTING, SP_CONNECT_TIMEOUT_MS))
    {
        sp_lock();
        sp_quiet_close = true;
        if (sp_link_pending)
            gap_connect_cancel();
        else if (sp_con != HCI_CON_HANDLE_INVALID)
            gap_disconnect(sp_con);
        sp_unlock();
        sp_wait_while(SP_CONNECTING, 3000);
        error("Serial device did not answer");
    }
    if (sp_state != SP_CONNECTED)
    {
        if (sp_no_service)
            error("The device has no $ service", (char *)sp_le_services[service].name);
        sp_error_status("Connection failed", sp_last_status);
    }

    MMPrintString("Connected to BLE serial device ");
    MMPrintString((char *)bd_addr_to_str(sp_addr));
    MMPrintString(" (");
    MMPrintString((char *)sp_le_services[service].name);
    MMPrintString(") as COM7");
    PRet();
}

void bt_serial_disconnect(void)
{
    sp_lock();
    sp_auto = false; /* until COM7 is opened again, or the next CONNECT */
    btstack_run_loop_remove_timer(&sp_timer);
    sp_update_scan_hold();
    sp_unlock();
    if (sp_state != SP_CONNECTED && sp_state != SP_CONNECTING)
        error("No serial device connected");
    sp_lock();
    sp_quiet_close = true;
    if (!sp_le)
        rfcomm_disconnect(sp_cid);
    else if (sp_link_pending)
        gap_connect_cancel();
    else if (sp_con != HCI_CON_HANDLE_INVALID)
        gap_disconnect(sp_con);
    sp_unlock();
    uint64_t end = time_us_64() + 5000000u;
    while (sp_state != SP_NONE)
    {
        { CheckAbort(); NetYield(); }
        if (time_us_64() > end)
            error("Serial device did not release the connection");
    }
}

void bt_serial_address(char *buf)
{
    if (sp_state == SP_CONNECTED)
        strcpy(buf, bd_addr_to_str(sp_addr));
    else
        buf[0] = 0;
}

void bt_serial_status(char *buf, int len)
{
    if (sp_state == SP_CONNECTED)
    {
        char how[48];
        if (sp_le)
            snprintf(how, sizeof(how), "BLE, %s", sp_le_services[sp_service].name);
        else
            snprintf(how, sizeof(how), "RFCOMM channel %u", (unsigned)sp_channel);
        snprintf(buf, len, "%s, connected (%s, COM7 %s), %lu bytes received, %lu sent",
                 bd_addr_to_str(sp_addr), how, com7 ? "open" : "closed",
                 (unsigned long)sp_received, (unsigned long)sp_sent);
    }
    else if (sp_state == SP_CONNECTING || sp_state == SP_QUERYING)
        snprintf(buf, len, "%s, connecting", bd_addr_to_str(sp_addr));
    else if (sp_have_remembered)
        snprintf(buf, len, "not connected (remembered %s%s%s)", bd_addr_to_str(sp_remembered.addr),
                 sp_remembered.le ? ", BLE" : "", sp_auto ? ", COM7 open - waiting for it" : "");
    else
        snprintf(buf, len, "none");
}

void bt_serial_forget(const uint8_t *addr)
{
    if ((sp_state == SP_CONNECTED || sp_state == SP_CONNECTING) &&
        (addr == NULL || memcmp(addr, sp_addr, sizeof(bd_addr_t)) == 0))
    {
        sp_quiet_close = true;
        if (!sp_le)
            rfcomm_disconnect(sp_cid);
        else if (sp_link_pending)
            gap_connect_cancel();
        else if (sp_con != HCI_CON_HANDLE_INVALID)
            gap_disconnect(sp_con);
    }
    if (sp_have_remembered && (addr == NULL || bt_serial_is_remembered(addr)))
    {
        const btstack_tlv_t *tlv;
        void *ctx;
        btstack_tlv_get_instance(&tlv, &ctx);
        if (tlv)
            tlv->delete_tag(ctx, SP_TLV_TAG);
        sp_have_remembered = false;
        sp_update_scan_hold();
    }
}

/* ============================================================================
 * COM7 (Serial.c, interpreter context)
 * ============================================================================
 */
bool bt_serial_known(void)
{
    return sp_state == SP_CONNECTED || sp_have_remembered;
}

void bt_serial_port_opened(void)
{
    sp_lock();
    sp_auto = true;
    sp_tx_tail = sp_tx_head;
    sp_reconnect_after(SP_RECONNECT_FIRST_MS); /* Classic; nothing happens while it is connected */
    sp_update_scan_hold();                      /* BLE */
    sp_unlock();
}

void bt_serial_port_closed(void)
{
    sp_lock();
    sp_auto = false;
    btstack_run_loop_remove_timer(&sp_timer);
    sp_update_scan_hold();
    sp_tx_tail = sp_tx_head;
    sp_unlock();
}

bool bt_serial_putc(uint8_t c)
{
    if (sp_state != SP_CONNECTED)
        return true; /* dropped, as for a USB CDC port with no device */
    uint16_t next = (uint16_t)((sp_tx_head + 1) % SP_TX_SIZE);
    if (next == sp_tx_tail)
        return false;
    sp_tx_buf[sp_tx_head] = c;
    sp_tx_head = next;
    if (!sp_send_requested)
    {
        sp_lock();
        if (sp_state == SP_CONNECTED && !sp_send_requested)
            sp_request_send();
        sp_unlock();
    }
    return true;
}

int bt_serial_tx_pending(void)
{
    if (sp_state != SP_CONNECTED)
        return 0; /* nothing will go */
    int n = sp_tx_head - sp_tx_tail;
    return n < 0 ? n + SP_TX_SIZE : n;
}

#endif /* PICOMITEBTH || PICOMITEHDMIBTH */
