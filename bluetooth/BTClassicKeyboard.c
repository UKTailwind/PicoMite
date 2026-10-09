/*
 * BTClassicKeyboard.c - Bluetooth Classic HID keyboards for the BT host
 * builds (PICOMITEBTH, PICOMITEHDMIBTH), and the touchpad or mouse built
 * into one.
 *
 * A Classic keyboard is found by BLUETOOTH SCAN (an inquiry: its Class of
 * Device says keyboard) and paired by BLUETOOTH CONNECT name$, which calls
 * bt_ckbd_connect(). The link runs in report protocol: boot protocol is
 * not reliable on such keyboards - one sent its touchpad as a boot mouse
 * report in its iOS mode but as its own report (ID 3, 16-bit X and Y) in
 * its Windows mode, whatever the host asked for. Where each report keeps
 * its fields is read from the keyboard's HID report descriptor when it is
 * paired (ck_parse_descriptor), and remembered with it: a keyboard that
 * reconnects by itself does not send its descriptor again. Keys go to
 * process_kbd_report(), the touchpad to process_mouse_input() and media
 * keys to process_consumer_report(), as USB and BLE keyboards' do:
 * OPTION KEYBOARD layouts, the lock keys, auto-repeat, KEYDOWN(),
 * DEVICE(MOUSE 2, ...) and the media-key codes all apply.
 *
 * Pairing is allowed only while CONNECT runs. For it the board says it has
 * a display, and asks for protection against eavesdropping, so a keyboard
 * that has keys gets Passkey Entry: the board shows six digits and they
 * are typed on the keyboard. A keyboard that only knows legacy PIN pairing
 * is answered with a random six-digit PIN, shown the same way; one that
 * declares no input gets Just Works. Speakers keep Just Works (BTAudio.c) -
 * the settings go back as soon as the keyboard is connected.
 *
 * After that the keyboard reconnects by itself (it pages the board when it
 * wakes or is switched on); only the remembered keyboard is accepted.
 *
 * Threading: the handlers run in btstack's context inside cyw43_arch_poll()
 * (bt_keyboard_poll) and never print - console messages go through
 * bt_notice(). bt_ckbd_connect() runs in the interpreter and takes the
 * async-context lock around btstack calls.
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

#include "btstack.h"
#include "btstack_tlv.h"
#include "btstack_hid_parser.h"

#include "BTClassicKeyboard.h"
#include "BTKeyboard.h"
#include "KeyboardMap.h"

#define CK_TLV_TAG 0x4254434Bu        /* 'BTCK': the remembered keyboard's address */
#define CK_TLV_LAYOUT 0x4254434Cu     /* 'BTCL': its report layout */
#define CK_CONNECT_TIMEOUT_MS 60000   /* the passkey is typed meanwhile */
/* The HID descriptor arrives into this while CONNECT pairs a keyboard. btstack
   keeps the pointer for every later connection, which can open while a
   program runs, so it cannot come from the MMBasic heap (NEW and RUN free
   that). A descriptor that does not fit leaves the boot report layout. */
#define CK_DESCRIPTOR_SIZE 384

/* Where a keyboard's reports keep their fields, from its HID report
   descriptor. Bit positions count from after the report ID. A size of 0
   means the field is absent. */
#define CK_LAYOUT_VERSION 2
#define CK_NO_POS 0xFFFFu
typedef struct
{
    uint8_t version;     /* CK_LAYOUT_VERSION */
    uint8_t has_ids;     /* the reports start with a report ID */
    uint8_t kbd_id;      /* keyboard: modifier byte (usages E0-E7), key slots */
    uint8_t kbd_keys;    /* 8-bit key slots, at most the 6 a boot report holds */
    uint16_t kbd_mod_pos;
    uint16_t kbd_keys_pos;
    uint8_t mouse_id;    /* touchpad or mouse: buttons, X, Y, wheel */
    uint8_t btn_id;
    uint8_t btn_count;
    uint8_t x_size;
    uint8_t y_size;
    uint8_t wheel_size;
    uint16_t btn_pos;
    uint16_t x_pos;
    uint16_t y_pos;
    uint16_t wheel_pos;
    uint8_t cons_id;     /* media keys: one consumer usage, as an array */
    uint8_t cons_size;
    uint16_t cons_pos;
    bt_led_layout_t led; /* the lock keys' LEDs (output report) */
    uint16_t desc_len;   /* BLUETOOTH STATUS */
} ck_layout_t;

typedef enum
{
    CK_NONE,
    CK_CONNECTING,
    CK_CONNECTED,
} ck_state_t;

static volatile ck_state_t ck_state;
static uint16_t ck_cid;
static bd_addr_t ck_addr;            /* the keyboard connecting or connected */
static bd_addr_t ck_remembered;
static bool ck_have_remembered;
static volatile bool ck_pairing;     /* CONNECT running: pairing allowed with ck_addr */
static volatile uint8_t ck_last_status;
static volatile uint32_t ck_reports; /* since connecting (BLUETOOTH STATUS) */
static ck_layout_t ck_layout;
static bool ck_have_layout;
static uint8_t ck_desc_status = 0xFF; /* the last descriptor's outcome (0 = read) */
static uint8_t ck_led_buf[8];        /* hid_host sends from it later */
static bool ck_led_pending;          /* LEDs to set with the next key report */

static uint8_t ck_descriptor_storage[CK_DESCRIPTOR_SIZE];
static btstack_packet_callback_registration_t ck_hci_registration;

static inline void ck_lock(void)
{
    async_context_acquire_lock_blocking(cyw43_arch_async_context());
}

static inline void ck_unlock(void)
{
    async_context_release_lock(cyw43_arch_async_context());
}

bool bt_ckbd_cod_is_keyboard(uint32_t cod)
{
    return ((cod >> 8) & 0x1F) == 0x05 && (cod & 0x40) != 0;
}

bool bt_ckbd_is_remembered(const uint8_t *addr)
{
    return ck_have_remembered && memcmp(addr, ck_remembered, sizeof(bd_addr_t)) == 0;
}

bool bt_ckbd_owns_pairing(const uint8_t *addr)
{
    return ck_pairing && memcmp(addr, ck_addr, sizeof(bd_addr_t)) == 0;
}

static void ck_store(uint32_t tag, const void *data, uint32_t len)
{
    const btstack_tlv_t *tlv;
    void *ctx;
    btstack_tlv_get_instance(&tlv, &ctx);
    if (tlv)
        tlv->store_tag(ctx, tag, data, len);
}

static void ck_remember(const bd_addr_t addr)
{
    ck_store(CK_TLV_TAG, addr, sizeof(bd_addr_t));
    memcpy(ck_remembered, addr, sizeof(bd_addr_t));
    ck_have_remembered = true;
}

/* Back to the speakers' pairing settings (see bt_audio_init). */
static void ck_pairing_done(void)
{
    ck_pairing = false;
    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    gap_ssp_set_authentication_requirement(SSP_IO_AUTHREQ_MITM_PROTECTION_NOT_REQUIRED_GENERAL_BONDING);
}

static void ck_passkey_notice(uint32_t passkey)
{
    char msg[96];
    snprintf(msg, sizeof(msg),
             "\r\nBluetooth keyboard pairing: type %06lu on the keyboard being paired, then Enter\r\n",
             (unsigned long)passkey);
    bt_notice(msg);
}

/* ============================================================================
 * Report layout, from the HID report descriptor
 * ============================================================================
 */
void bt_hid_led_layout(const uint8_t *descriptor, uint16_t len, bt_led_layout_t *l)
{
    uint16_t end = 0;
    memset(l, 0, sizeof(*l));
    l->pos[0] = l->pos[1] = l->pos[2] = CK_NO_POS;

    btstack_hid_usage_iterator_t it;
    btstack_hid_usage_item_t item;
    btstack_hid_usage_iterator_init(&it, descriptor, len, HID_REPORT_TYPE_OUTPUT);
    while (btstack_hid_usage_iterator_has_more(&it))
    {
        btstack_hid_usage_iterator_get_item(&it, &item);
        bool variable = (item.descriptor_item.item_value & 2) != 0;
        uint8_t id = item.report_id == 0xFFFF ? 0 : (uint8_t)item.report_id;
        if (!l->present)
        {
            /* LED page: Num Lock 1, Caps Lock 2, Scroll Lock 3 */
            if (item.usage_page != 0x08 || !variable || item.size != 1 || item.usage < 1 || item.usage > 3)
                continue;
            l->present = 1;
            l->has_id = item.report_id != 0xFFFF;
            l->id = id;
        }
        if (id != l->id)
            continue;
        if (item.usage_page == 0x08 && variable && item.size == 1 && item.usage >= 1 && item.usage <= 3)
            l->pos[item.usage - 1] = item.bit_pos;
        if (item.bit_pos + item.size > end)
            end = item.bit_pos + item.size;
    }
    l->len = (uint8_t)((end + 7) / 8 > 8 ? 8 : (end + 7) / 8);
    if (!l->len)
        l->present = 0;
}

uint8_t bt_hid_led_report(const bt_led_layout_t *l, uint8_t leds, uint8_t *out)
{
    memset(out, 0, 8);
    for (int i = 0; i < 3; i++)
        if ((leds & (1u << i)) && l->pos[i] != CK_NO_POS && l->pos[i] < l->len * 8u)
            out[l->pos[i] >> 3] |= (uint8_t)(1u << (l->pos[i] & 7));
    return l->len;
}

static void ck_parse_descriptor(const uint8_t *d, uint16_t len, ck_layout_t *l)
{
    memset(l, 0, sizeof(*l));
    l->version = CK_LAYOUT_VERSION;
    l->kbd_mod_pos = l->kbd_keys_pos = CK_NO_POS;
    l->desc_len = len;

    btstack_hid_usage_iterator_t it;
    btstack_hid_usage_item_t item;
    btstack_hid_usage_iterator_init(&it, d, len, HID_REPORT_TYPE_INPUT);
    while (btstack_hid_usage_iterator_has_more(&it))
    {
        btstack_hid_usage_iterator_get_item(&it, &item);
        bool variable = (item.descriptor_item.item_value & 2) != 0;
        uint8_t id = 0;
        if (item.report_id != 0xFFFF)
        {
            l->has_ids = 1;
            id = (uint8_t)item.report_id;
        }
        switch (item.usage_page)
        {
        case 0x01: /* Generic Desktop */
            if (!variable)
                break;
            if (item.usage == 0x30 && !l->x_size)
            {
                l->mouse_id = id;
                l->x_pos = item.bit_pos;
                l->x_size = item.size;
            }
            else if (item.usage == 0x31 && !l->y_size && l->x_size && id == l->mouse_id)
            {
                l->y_pos = item.bit_pos;
                l->y_size = item.size;
            }
            else if (item.usage == 0x38 && !l->wheel_size && l->x_size && id == l->mouse_id)
            {
                l->wheel_pos = item.bit_pos;
                l->wheel_size = item.size;
            }
            break;

        case 0x09: /* Button */
            if (!variable || item.size != 1 || item.usage < 1 || item.usage > 8)
                break;
            if (!l->btn_count)
            {
                l->btn_id = id;
                l->btn_pos = (uint16_t)(item.bit_pos - (item.usage - 1));
            }
            if (id == l->btn_id && item.usage > l->btn_count)
                l->btn_count = (uint8_t)item.usage;
            break;

        case 0x0C: /* Consumer: a usage array; bitmaps are not decoded */
            if (!variable && item.size >= 8 && item.size <= 16 && !l->cons_size)
            {
                l->cons_id = id;
                l->cons_pos = item.bit_pos;
                l->cons_size = item.size;
            }
            break;

        default:
            break;
        }
    }
    if (!l->y_size)
        l->x_size = 0; /* not a pointer */
    if (l->btn_count && l->btn_id != l->mouse_id)
        l->btn_count = 0;
    bt_kbd_layout_t k;
    bt_hid_kbd_layout(d, len, &k);
    if (k.present)
    {
        l->kbd_id = k.id;
        l->kbd_keys = k.keys;
        l->kbd_mod_pos = k.mod_pos;
        l->kbd_keys_pos = k.keys_pos;
    }
    bt_hid_led_layout(d, len, &l->led);
}

/* The lock keys' LEDs. A keyboard lights its LED for a moment and turns it
   off again unless the host sets it. Sent with the first key report after
   connecting (the link is idle then; hid_host_send_report would cut into
   the connection's own setup) and whenever a lock key is pressed on it.
   The boot keyboard's output report (ID 1, one byte) when there is no
   layout. */
static void ck_send_leds(void)
{
    ck_led_pending = false;
    if (ck_state != CK_CONNECTED)
        return;
    uint8_t len;
    uint16_t id;
    if (ck_have_layout)
    {
        if (!ck_layout.led.present)
            return;
        len = bt_hid_led_report(&ck_layout.led, kbd_lock_leds(), ck_led_buf);
        id = ck_layout.led.has_id ? ck_layout.led.id : HID_REPORT_ID_UNDEFINED;
    }
    else
    {
        ck_led_buf[0] = kbd_lock_leds(); /* the boot keyboard's output report */
        len = 1;
        id = 1;
    }
    /* Tried again with the next report while hid_host is busy. */
    ck_led_pending = hid_host_send_report(ck_cid, id, ck_led_buf, len) == ERROR_CODE_COMMAND_DISALLOWED;
}

void bt_ckbd_set_leds(uint8_t leds)
{
    (void)leds; /* ck_send_leds() reads them, as for the first report */
    ck_send_leds();
}

static uint32_t ck_bits(const uint8_t *p, uint16_t n, uint16_t pos, uint8_t size)
{
    uint32_t v = 0;
    if ((uint32_t)pos + size > (uint32_t)n * 8u)
        return 0;
    for (uint8_t i = 0; i < size; i++)
        if (p[(pos + i) >> 3] & (1u << ((pos + i) & 7)))
            v |= 1u << i;
    return v;
}

static int32_t ck_sbits(const uint8_t *p, uint16_t n, uint16_t pos, uint8_t size)
{
    uint32_t v = ck_bits(p, n, pos, size);
    if (size && size < 32 && (v & (1u << (size - 1))))
        v |= ~0u << size;
    return (int32_t)v;
}

static int32_t ck_clamp(int32_t v, int32_t lo, int32_t hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

void bt_hid_kbd_layout(const uint8_t *descriptor, uint16_t len, bt_kbd_layout_t *k)
{
    /* each input report's length, to tell whether the keyboard's is unique */
    uint8_t ids[16];
    uint16_t ends[16];
    int nid = 0;
    memset(k, 0, sizeof(*k));
    k->mod_pos = k->keys_pos = CK_NO_POS;

    btstack_hid_usage_iterator_t it;
    btstack_hid_usage_item_t item;
    btstack_hid_usage_iterator_init(&it, descriptor, len, HID_REPORT_TYPE_INPUT);
    while (btstack_hid_usage_iterator_has_more(&it))
    {
        btstack_hid_usage_iterator_get_item(&it, &item);
        bool variable = (item.descriptor_item.item_value & 2) != 0;
        uint8_t id = item.report_id == 0xFFFF ? 0 : (uint8_t)item.report_id;
        int j = 0;
        while (j < nid && ids[j] != id)
            j++;
        if (j == nid && nid < 16)
        {
            ids[nid] = id;
            ends[nid++] = 0;
        }
        if (j < nid && item.bit_pos + item.size > ends[j])
            ends[j] = item.bit_pos + item.size;
        if (item.usage_page != 0x07) /* Keyboard */
            continue;
        if (variable && item.size == 1 && item.usage == 0xE0 && k->mod_pos == CK_NO_POS)
        {
            if (k->keys && id != k->id)
                continue;
            k->id = id;
            k->has_id = item.report_id != 0xFFFF;
            k->mod_pos = item.bit_pos;
        }
        else if (!variable && item.size == 8)
        {
            if (k->keys == 0 && (k->mod_pos == CK_NO_POS || id == k->id))
            {
                k->id = id;
                k->has_id = item.report_id != 0xFFFF;
                k->keys_pos = item.bit_pos;
                k->keys = 1;
            }
            else if (k->keys && k->keys < 6 && id == k->id && item.bit_pos == k->keys_pos + 8u * k->keys)
                k->keys++;
        }
    }
    if (!k->keys)
        return;
    k->present = 1;
    for (int j = 0; j < nid; j++)
        if (ids[j] == k->id)
            k->len = (uint8_t)((ends[j] + 7) / 8);
    k->len_unique = 1;
    for (int j = 0; j < nid; j++)
        if (ids[j] != k->id && (ends[j] + 7) / 8 == k->len)
            k->len_unique = 0;
}

void bt_hid_kbd_report(const bt_kbd_layout_t *k, const uint8_t *p, uint16_t n, uint8_t out[8])
{
    memset(out, 0, 8);
    if (k->mod_pos != CK_NO_POS)
        out[0] = (uint8_t)ck_bits(p, n, k->mod_pos, 8);
    for (int i = 0; i < k->keys; i++)
        out[2 + i] = (uint8_t)ck_bits(p, n, (uint16_t)(k->keys_pos + 8 * i), 8);
}

/* An input report: transaction header 0xA1 (DATA, input), then the report. */
static void ck_report(const uint8_t *r, uint16_t len)
{
    if (len < 2 || r[0] != 0xA1)
        return;
    ck_reports++;
    const uint8_t *p = r + 1;
    uint16_t n = len - 1;

    if (!ck_have_layout)
    {
        /* No descriptor read: the boot protocol's keyboard (ID 1) and mouse
           (ID 2) reports. */
        if (p[0] == 1 && n >= 9)
        {
            process_kbd_report((const hid_keyboard_report_t *)(p + 1), KBD_SOURCE_BT_CLASSIC);
            if (ck_led_pending)
                ck_send_leds();
        }
        else if (p[0] == 2 && n >= 4) /* buttons, X, Y [, wheel] */
            process_mouse_input((int8_t)p[2], (int8_t)p[3], n >= 5 ? (int8_t)p[4] : 0, p[1], 2);
        return;
    }

    const ck_layout_t *l = &ck_layout;
    uint8_t id = 0;
    if (l->has_ids)
    {
        id = p[0];
        p++;
        n--;
    }
    if (l->kbd_keys && id == l->kbd_id)
    {
        hid_keyboard_report_t k;
        memset(&k, 0, sizeof(k));
        if (l->kbd_mod_pos != CK_NO_POS)
            k.modifier = (uint8_t)ck_bits(p, n, l->kbd_mod_pos, 8);
        for (int i = 0; i < l->kbd_keys; i++)
            k.keycode[i] = (uint8_t)ck_bits(p, n, (uint16_t)(l->kbd_keys_pos + 8 * i), 8);
        process_kbd_report(&k, KBD_SOURCE_BT_CLASSIC);
        if (ck_led_pending)
            ck_send_leds();
    }
    if (l->x_size && id == l->mouse_id)
    {
        int32_t x = ck_sbits(p, n, l->x_pos, l->x_size);
        int32_t y = ck_sbits(p, n, l->y_pos, l->y_size);
        int32_t w = l->wheel_size ? ck_sbits(p, n, l->wheel_pos, l->wheel_size) : 0;
        uint8_t b = l->btn_count ? (uint8_t)ck_bits(p, n, l->btn_pos, l->btn_count) : 0;
        process_mouse_input((int16_t)ck_clamp(x, -32768, 32767), (int16_t)ck_clamp(y, -32768, 32767),
                            (int8_t)ck_clamp(w, -128, 127), b, 2);
    }
    if (l->cons_size && id == l->cons_id)
    {
        uint32_t u = ck_bits(p, n, l->cons_pos, l->cons_size);
        uint8_t usage[2] = {(uint8_t)u, (uint8_t)(u >> 8)};
        process_consumer_report(usage, 2);
    }
}

/* ============================================================================
 * btstack handlers
 * ============================================================================
 */
/* Pairing requests for the keyboard CONNECT is pairing; BTAudio.c's handler
   leaves these alone (bt_ckbd_owns_pairing). */
static void ck_hci_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
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
            if (tlv && tlv->get_tag(ctx, CK_TLV_TAG, ck_remembered, sizeof(bd_addr_t)) == sizeof(bd_addr_t))
                ck_have_remembered = true;
            if (tlv && tlv->get_tag(ctx, CK_TLV_LAYOUT, (uint8_t *)&ck_layout, sizeof(ck_layout)) == sizeof(ck_layout) &&
                ck_layout.version == CK_LAYOUT_VERSION)
                ck_have_layout = true;
        }
        break;

    case HCI_EVENT_USER_PASSKEY_NOTIFICATION:
        hci_event_user_passkey_notification_get_bd_addr(packet, addr);
        if (bt_ckbd_owns_pairing(addr))
            ck_passkey_notice(hci_event_user_passkey_notification_get_numeric_value(packet));
        break;

    case HCI_EVENT_PIN_CODE_REQUEST:
        /* Legacy pairing: the PIN is ours to choose, typed on the keyboard. */
        hci_event_pin_code_request_get_bd_addr(packet, addr);
        if (bt_ckbd_owns_pairing(addr))
        {
            uint32_t pin = (time_us_32() ^ (time_us_32() >> 11) * 2654435761u) % 1000000u;
            char pin_str[8];
            snprintf(pin_str, sizeof(pin_str), "%06lu", (unsigned long)pin);
            gap_pin_code_response(addr, pin_str);
            ck_passkey_notice(pin);
        }
        break;

    case HCI_EVENT_USER_CONFIRMATION_REQUEST:
        /* Just Works, if the keyboard insists on it. */
        hci_event_user_confirmation_request_get_bd_addr(packet, addr);
        if (bt_ckbd_owns_pairing(addr))
            gap_ssp_confirmation_response(addr);
        break;

    default:
        break;
    }
}

static void ck_hid_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    (void)channel;
    (void)size;
    if (packet_type != HCI_EVENT_PACKET || hci_event_packet_get_type(packet) != HCI_EVENT_HID_META)
        return;

    switch (hci_event_hid_meta_get_subevent_code(packet))
    {
    case HID_SUBEVENT_INCOMING_CONNECTION:
    {
        /* The remembered keyboard, woken or switched on, pages us. */
        bd_addr_t addr;
        uint16_t cid = hid_subevent_incoming_connection_get_hid_cid(packet);
        hid_subevent_incoming_connection_get_address(packet, addr);
        if (ck_state == CK_NONE && bt_ckbd_is_remembered(addr))
        {
            memcpy(ck_addr, addr, sizeof(bd_addr_t));
            ck_cid = cid;
            ck_state = CK_CONNECTING;
            hid_host_accept_connection(cid, HID_PROTOCOL_MODE_REPORT);
        }
        else
            hid_host_decline_connection(cid);
        break;
    }

    case HID_SUBEVENT_CONNECTION_OPENED:
    {
        if (hid_subevent_connection_opened_get_hid_cid(packet) != ck_cid)
            break;
        uint8_t status = hid_subevent_connection_opened_get_status(packet);
        bool was_pairing = ck_pairing;
        if (ck_pairing)
            ck_pairing_done();
        if (status != ERROR_CODE_SUCCESS)
        {
            ck_last_status = status;
            ck_state = CK_NONE;
            break;
        }
        ck_reports = 0;
        ck_led_pending = true;
        ck_state = CK_CONNECTED;
        if (was_pairing)
            ck_remember(ck_addr); /* here, not in CONNECT: Ctrl-C may have left it */
        else if (!CurrentLinePtr)
            bt_notice("Bluetooth Keyboard Connected\r\n> ");
        break;
    }

    case HID_SUBEVENT_DESCRIPTOR_AVAILABLE:
    {
        /* Only a connection the board opened reads the descriptor: CONNECT.
           The layout replaces the last keyboard's. */
        uint16_t cid = hid_subevent_descriptor_available_get_hid_cid(packet);
        if (cid != ck_cid)
            break;
        ck_desc_status = hid_subevent_descriptor_available_get_status(packet);
        ck_have_layout = false;
        if (ck_desc_status == ERROR_CODE_SUCCESS)
        {
            ck_parse_descriptor(hid_descriptor_storage_get_descriptor_data(cid),
                                hid_descriptor_storage_get_descriptor_len(cid), &ck_layout);
            ck_have_layout = ck_layout.kbd_keys || ck_layout.x_size || ck_layout.cons_size;
        }
        if (ck_have_layout)
            ck_store(CK_TLV_LAYOUT, &ck_layout, sizeof(ck_layout));
        else
        {
            const btstack_tlv_t *tlv;
            void *ctx;
            btstack_tlv_get_instance(&tlv, &ctx);
            if (tlv)
                tlv->delete_tag(ctx, CK_TLV_LAYOUT); /* not the last keyboard's layout */
        }
        break;
    }

    case HID_SUBEVENT_CONNECTION_CLOSED:
        if (hid_subevent_connection_closed_get_hid_cid(packet) != ck_cid)
            break;
        if (ck_pairing)
            ck_pairing_done();
        if (ck_state == CK_CONNECTED && !CurrentLinePtr)
            bt_notice("Bluetooth Keyboard Disconnected\r\n> ");
        ck_state = CK_NONE;
        break;

    case HID_SUBEVENT_REPORT:
        if (hid_subevent_report_get_hid_cid(packet) == ck_cid)
            ck_report(hid_subevent_report_get_report(packet), hid_subevent_report_get_report_len(packet));
        break;

    default:
        break;
    }
}

void bt_ckbd_init(void)
{
    ck_state = CK_NONE;
    hid_host_init(ck_descriptor_storage, sizeof(ck_descriptor_storage));
    hid_host_register_packet_handler(&ck_hid_handler);
    ck_hci_registration.callback = &ck_hci_handler;
    hci_add_event_handler(&ck_hci_registration);
}

/* ============================================================================
 * BLUETOOTH CONNECT, STATUS, FORGET and MM.INFO
 * ============================================================================
 */
/* Wait in the interpreter, keeping Ctrl-C and the background services
   alive. Returns false on timeout. */
static bool ck_wait_not_connecting(uint32_t timeout_ms)
{
    uint64_t end = time_us_64() + (uint64_t)timeout_ms * 1000u;
    while (ck_state == CK_CONNECTING)
    {
        CheckAbort();
        if (time_us_64() > end)
            return false;
    }
    return true;
}

void bt_ckbd_connect(const uint8_t *addr)
{
    if (ck_state != CK_NONE)
        error(ck_state == CK_CONNECTING ? "Keyboard already connecting" : "Keyboard already connected");

    uint8_t status;
    ck_lock();
    memcpy(ck_addr, addr, sizeof(bd_addr_t));
    ck_last_status = 0;
    ck_desc_status = 0xFF;
    /* A display, and protection against eavesdropping: Passkey Entry. */
    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_ONLY);
    gap_ssp_set_authentication_requirement(SSP_IO_AUTHREQ_MITM_PROTECTION_REQUIRED_GENERAL_BONDING);
    ck_pairing = true;
    status = hid_host_connect(ck_addr, HID_PROTOCOL_MODE_REPORT, &ck_cid);
    if (status == ERROR_CODE_SUCCESS)
        ck_state = CK_CONNECTING;
    else
        ck_pairing_done();
    ck_unlock();
    if (status != ERROR_CODE_SUCCESS)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "Connect failed (Bluetooth error 0x%02X)", status);
        error("$", buf);
    }

    bool answered = ck_wait_not_connecting(CK_CONNECT_TIMEOUT_MS);
    if (!answered)
    {
        ck_lock();
        ck_pairing_done();
        hid_host_disconnect(ck_cid);
        ck_unlock();
        ck_state = CK_NONE;
        error("Keyboard did not answer");
    }
    if (ck_state != CK_CONNECTED)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "Connection failed (Bluetooth error 0x%02X)", ck_last_status);
        error("$", buf);
    }

    MMPrintString("Connected to keyboard ");
    MMPrintString((char *)bd_addr_to_str(ck_addr));
    PRet();
}

void bt_ckbd_address(char *buf)
{
    if (ck_state == CK_CONNECTED)
        strcpy(buf, bd_addr_to_str(ck_addr));
    else
        buf[0] = 0;
}

static int ck_status_id(char *buf, int len, const char *what, bool present, uint8_t id)
{
    if (!present)
        return snprintf(buf, len, " %s -", what);
    if (!ck_layout.has_ids)
        return snprintf(buf, len, " %s", what);
    return snprintf(buf, len, " %s %u", what, (unsigned)id);
}

void bt_ckbd_status(char *buf, int len)
{
    int n;
    if (ck_state == CK_CONNECTED)
        n = snprintf(buf, len, "%s, connected (%lu reports)", bd_addr_to_str(ck_addr), (unsigned long)ck_reports);
    else if (ck_state == CK_CONNECTING)
        n = snprintf(buf, len, "%s, connecting", bd_addr_to_str(ck_addr));
    else if (ck_have_remembered)
        n = snprintf(buf, len, "not connected (remembered %s)", bd_addr_to_str(ck_remembered));
    else
    {
        snprintf(buf, len, "none");
        return;
    }
    if (n < 0 || n >= len)
        return;
    /* The report IDs carrying each kind of input, or the boot layout. */
    if (!ck_have_layout)
    {
        snprintf(buf + n, len - n, "; boot reports%s",
                 ck_desc_status == 0xFF || ck_desc_status == ERROR_CODE_SUCCESS ? "" : " (descriptor not read)");
        return;
    }
    n += snprintf(buf + n, len - n, "; reports:");
    if (n < len)
        n += ck_status_id(buf + n, len - n, "keys", ck_layout.kbd_keys != 0, ck_layout.kbd_id);
    if (n < len)
        n += ck_status_id(buf + n, len - n, "pointer", ck_layout.x_size != 0, ck_layout.mouse_id);
    if (n < len)
        n += ck_status_id(buf + n, len - n, "media", ck_layout.cons_size != 0, ck_layout.cons_id);
    if (n < len)
        snprintf(buf + n, len - n, " (descriptor %u bytes)", (unsigned)ck_layout.desc_len);
}

void bt_ckbd_forget(const uint8_t *addr)
{
    if (ck_state != CK_NONE && (addr == NULL || memcmp(addr, ck_addr, sizeof(bd_addr_t)) == 0))
        hid_host_disconnect(ck_cid);
    if (ck_have_remembered && (addr == NULL || memcmp(addr, ck_remembered, sizeof(bd_addr_t)) == 0))
    {
        const btstack_tlv_t *tlv;
        void *ctx;
        btstack_tlv_get_instance(&tlv, &ctx);
        if (tlv)
        {
            tlv->delete_tag(ctx, CK_TLV_TAG);
            tlv->delete_tag(ctx, CK_TLV_LAYOUT);
        }
        ck_have_remembered = false;
        ck_have_layout = false;
    }
}

#endif /* PICOMITEBTH || PICOMITEHDMIBTH */
