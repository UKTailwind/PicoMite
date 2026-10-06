/*
 * BTAudio.c — Bluetooth Classic A2DP source (speaker / headphone output) for
 * the BT host builds (PICOMITEBTH, PICOMITEHDMIBTH).
 *
 * Phase 1 of docs/Bluetooth_Audio_Plan.md: the dual-mode btstack, finding and
 * connecting a speaker, and a test tone encoded to SBC on btstack's 10 ms
 * timer, after btstack's example/a2dp_source_demo.c. PLAY is not routed here
 * yet - that is phase 2.
 *
 * Threading: btstack runs in the async context's lowest-priority IRQ on core 0
 * (pico_cyw43_arch_none is threadsafe_background). Every handler below runs
 * there. cmd_bluetooth() runs in the interpreter and holds the async-context
 * lock around each call into btstack; it only reads the volatile state the
 * handlers publish.
 */

#if defined(PICOMITEBTH) || defined(PICOMITEHDMIBTH)

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "configuration.h"
#include "MMBasic_Includes.h"
#include "Hardware_Includes.h"

#include "pico/cyw43_arch.h"
#undef UNUSED
#include "btstack.h"
#include "classic/btstack_sbc_bluedroid.h"

#include "BTAudio.h"
#include "BTKeyboard.h"

#define BTA_AUDIO_TIMEOUT_MS 5    /* encode/send tick */
#define BTA_SBC_STORAGE_SIZE 1030 /* one media packet: header byte + SBC frames */
#define BTA_PREFERRED_RATE 44100
#define BTA_MAX_FOUND 16
#define BTA_NAME_LEN 32

/* ============================================================================
 * State published by the btstack handlers
 * ============================================================================
 */
typedef enum
{
    SPK_NONE,       /* no A2DP connection */
    SPK_CONNECTING, /* a2dp_source_establish_stream() issued */
    SPK_OPEN,       /* stream configured, not playing */
    SPK_STREAMING,  /* media packets flowing */
} spk_state_t;

static volatile spk_state_t spk_state;
static volatile uint8_t spk_last_status; /* last failure reported by btstack */
static volatile bool spk_quiet_release;  /* a command is waiting for the release */
static volatile bool spk_pairing_allowed; /* BLUETOOTH CONNECT is running */
static bool bta_ready;                   /* bt_audio_init() completed */

static uint16_t a2dp_cid;
static uint8_t local_seid;
static bd_addr_t speaker_addr;

/* Negotiated SBC configuration */
static volatile uint32_t cfg_rate;
static uint8_t cfg_channel_mode; /* btstack_sbc_channel_mode_t */
static uint8_t cfg_bitpool, cfg_blocks, cfg_subbands;

/* Offer 44.1 and 48 kHz, stereo or joint stereo, any block length / subband
   count / allocation, bitpool 2..53 - the sink picks within that. */
static const uint8_t media_sbc_codec_capabilities[] = {
    (AVDTP_SBC_44100 << 4) | (AVDTP_SBC_48000 << 4) | AVDTP_SBC_STEREO | AVDTP_SBC_JOINT_STEREO,
    0xFF,
    2, 53};
static uint8_t media_sbc_codec_configuration[4];
static uint8_t sdp_a2dp_source_service_buffer[150];

static btstack_packet_callback_registration_t hci_event_callback_registration;

/* ============================================================================
 * Inquiry (BLUETOOTH SCAN) results
 * ============================================================================
 */
typedef struct
{
    bd_addr_t addr;
    uint32_t cod;
    int8_t rssi;
    uint8_t psrm;
    uint16_t clock_offset;
    char name[BTA_NAME_LEN];
} bta_device_t;

static bta_device_t found[BTA_MAX_FOUND];
static volatile int found_count;
static volatile bool inquiry_active;
static volatile bool name_request_active;

/* ============================================================================
 * Encoder and media-sending state
 * ============================================================================
 */
static const btstack_sbc_encoder_t *sbc_encoder;
static btstack_sbc_encoder_bluedroid_t sbc_encoder_state;
static uint8_t sbc_storage[BTA_SBC_STORAGE_SIZE];
static uint16_t sbc_storage_count;
static bool sbc_ready_to_send;
static int16_t pcm_frame[16 * 8 * 2]; /* one SBC frame: 16 blocks x 8 subbands, stereo */

static btstack_timer_source_t audio_timer;
static uint32_t time_audio_data_sent;
static uint32_t acc_num_missed_samples;
static uint32_t samples_ready;
static uint32_t rtp_timestamp;
static int max_media_payload_size;

/* Encoder load: time spent in the tick handler over the time streamed. */
static volatile uint32_t load_busy_us;
static volatile uint32_t load_start_us;
static volatile uint32_t packets_sent;

/* Test tone (BLUETOOTH TEST freq). The tone fades in and out over
   BTA_RAMP_MS, and a fresh stream carries BTA_LEAD_IN_MS of silence before
   any tone, so a click heard at start/stop can be told apart from the
   speaker's own amplifier switching on and off with the stream. */
#define BTA_RAMP_MS 20
#define BTA_LEAD_IN_MS 200
#define BTA_GAIN_FULL 32768 /* Q15 */
static int16_t sine_table[256];
static bool sine_ready;
static volatile uint32_t tone_step;
static volatile int32_t tone_target; /* 0 or BTA_GAIN_FULL */
static volatile int32_t tone_gain;   /* current Q15 gain */
static uint32_t tone_phase;
static uint32_t lead_in_samples;

/* ============================================================================
 * Helpers
 * ============================================================================
 */
static inline void bta_lock(void)
{
    async_context_acquire_lock_blocking(cyw43_arch_async_context());
}

static inline void bta_unlock(void)
{
    async_context_release_lock(cyw43_arch_async_context());
}

/* Wait in the interpreter until cond() holds, keeping Ctrl-C and the
   background services alive. Returns false on timeout. */
static bool bta_wait(bool (*cond)(void), uint32_t timeout_ms)
{
    uint64_t end = time_us_64() + (uint64_t)timeout_ms * 1000u;
    while (!cond())
    {
        CheckAbort();
        if (time_us_64() > end)
            return false;
    }
    return true;
}

static bool cond_inquiry_done(void) { return !inquiry_active; }
static bool cond_name_done(void) { return !name_request_active; }
static bool cond_not_connecting(void) { return spk_state != SPK_CONNECTING; }
static bool cond_streaming(void) { return spk_state == SPK_STREAMING || spk_state == SPK_NONE; }
static bool cond_not_streaming(void) { return spk_state != SPK_STREAMING; }
static bool cond_released(void) { return spk_state == SPK_NONE; }

/* Major device class Audio/Video (0x04): speakers, headphones, car kits. */
static bool cod_is_audio(uint32_t cod)
{
    return ((cod >> 8) & 0x1F) == 0x04;
}

bool bt_audio_streaming(void)
{
    return spk_state == SPK_STREAMING;
}

/* ============================================================================
 * Audio production and SBC encoding (async-context IRQ)
 * ============================================================================
 */
static void produce_audio(int16_t *pcm, int frames)
{
    /* PLAY output (OPTION AUDIO BLUETOOTH) unless the test tone is sounding. */
    if (AUDIO_BLUETOOTH && tone_target == 0 && tone_gain == 0)
    {
        bt_audio_pull(pcm, frames, cfg_rate);
        return;
    }
    uint32_t step = tone_step;
    int32_t target = tone_target;
    int32_t gain = tone_gain;
    int32_t ramp = BTA_GAIN_FULL / (int32_t)(BTA_RAMP_MS * cfg_rate / 1000);
    for (int i = 0; i < frames; i++)
    {
        int16_t s = 0;
        if (lead_in_samples)
            lead_in_samples--;
        else
        {
            if (gain < target)
                gain = (gain + ramp > target) ? target : gain + ramp;
            else if (gain > target)
                gain = (gain - ramp < target) ? target : gain - ramp;
            if (gain)
            {
                s = (int16_t)((sine_table[tone_phase >> 24] * gain) >> 15);
                tone_phase += step;
            }
        }
        pcm[2 * i] = s;
        pcm[2 * i + 1] = s;
    }
    tone_gain = gain;
}

/* At most this many samples are taken from PLAY's swing buffers per tick:
   each of those buffers must hold a whole burst - see TONE_BUFFER_SIZE in
   Audio.c. A tick needs ~220 at 44.1 kHz, but a tick that falls while a
   packet waits to be sent encodes nothing, so the next must catch up; the
   payload limit alone would allow up to 12 frames (1536 samples) at low
   bitpools. */
#define BTA_MAX_SAMPLES_PER_TICK 1024

static void fill_sbc_audio_buffer(void)
{
    unsigned int frames_per_sbc = sbc_encoder->num_audio_frames(&sbc_encoder_state);
    uint16_t sbc_len = sbc_encoder->sbc_buffer_length(&sbc_encoder_state);
    unsigned int taken = 0;
    while (samples_ready >= frames_per_sbc &&
           (max_media_payload_size - sbc_storage_count) >= sbc_len &&
           taken + frames_per_sbc <= BTA_MAX_SAMPLES_PER_TICK)
    {
        produce_audio(pcm_frame, frames_per_sbc);
        sbc_encoder->encode_signed_16(&sbc_encoder_state, pcm_frame,
                                      &sbc_storage[1 + sbc_storage_count]);
        sbc_storage_count += sbc_len;
        samples_ready -= frames_per_sbc;
        taken += frames_per_sbc;
    }
}

/* Diagnostics (BLUETOOTH STATUS): the longest wait for btstack to let a
   packet go, and samples thrown away because sending fell behind. */
static volatile uint32_t send_wait_max_us, samples_dropped;
static uint32_t send_requested_us;

static void send_media_packet(void)
{
    uint32_t waited = time_us_32() - send_requested_us;
    if (waited > send_wait_max_us)
        send_wait_max_us = waited;
    int frame_len = sbc_encoder->sbc_buffer_length(&sbc_encoder_state);
    uint8_t num_frames = sbc_storage_count / frame_len;
    sbc_storage[0] = num_frames; /* SBC media payload header: frame count */
    a2dp_source_stream_send_media_payload_rtp(a2dp_cid, local_seid, 0, rtp_timestamp,
                                              sbc_storage, sbc_storage_count + 1);
    rtp_timestamp += num_frames * sbc_encoder->num_audio_frames(&sbc_encoder_state);
    sbc_storage_count = 0;
    sbc_ready_to_send = false;
    packets_sent++;
}

static volatile uint32_t tick_gap_max_us; /* longest gap between ticks (BLUETOOTH STATUS) */
static uint32_t tick_last_us;

static void audio_timeout_handler(btstack_timer_source_t *timer)
{
    uint32_t t0 = time_us_32();
    if (tick_last_us && t0 - tick_last_us > tick_gap_max_us)
        tick_gap_max_us = t0 - tick_last_us;
    tick_last_us = t0;
    btstack_run_loop_set_timer(timer, BTA_AUDIO_TIMEOUT_MS);
    btstack_run_loop_add_timer(timer);

    uint32_t now = btstack_run_loop_get_time_ms();
    uint32_t period = BTA_AUDIO_TIMEOUT_MS;
    if (time_audio_data_sent > 0)
        period = now - time_audio_data_sent;
    uint32_t rate = cfg_rate;
    uint32_t num = (period * rate) / 1000;
    acc_num_missed_samples += (period * rate) % 1000;
    while (acc_num_missed_samples >= 1000)
    {
        num++;
        acc_num_missed_samples -= 1000;
    }
    time_audio_data_sent = now;
    samples_ready += num;
    /* After a long stall (flash writes run with interrupts off) don't try
       to catch up more than a quarter of a second - the speaker has long
       since played silence for the gap. */
    if (samples_ready > rate / 4)
    {
        samples_dropped += samples_ready - rate / 4;
        samples_ready = rate / 4;
    }

    if (!sbc_ready_to_send)
    {
        fill_sbc_audio_buffer();
        if ((sbc_storage_count + sbc_encoder->sbc_buffer_length(&sbc_encoder_state)) >
            (uint32_t)max_media_payload_size)
        {
            sbc_ready_to_send = true;
            send_requested_us = time_us_32();
            a2dp_source_stream_endpoint_request_can_send_now(a2dp_cid, local_seid);
        }
    }
    load_busy_us += time_us_32() - t0;
}

static void audio_timer_start(void)
{
    /* sbc_storage[0] is the payload header, so frames fit in SIZE - 1. */
    max_media_payload_size = btstack_min(a2dp_max_media_payload_size(a2dp_cid, local_seid),
                                         BTA_SBC_STORAGE_SIZE - 1);
    sbc_storage_count = 0;
    sbc_ready_to_send = false;
    time_audio_data_sent = 0;
    acc_num_missed_samples = 0;
    samples_ready = 0;
    load_busy_us = 0;
    load_start_us = time_us_32();
    packets_sent = 0;
    btstack_run_loop_remove_timer(&audio_timer);
    btstack_run_loop_set_timer_handler(&audio_timer, audio_timeout_handler);
    btstack_run_loop_set_timer(&audio_timer, BTA_AUDIO_TIMEOUT_MS);
    btstack_run_loop_add_timer(&audio_timer);
}

static void audio_timer_stop(void)
{
    tick_last_us = 0;
    btstack_run_loop_remove_timer(&audio_timer);
    sbc_storage_count = 0;
    sbc_ready_to_send = false;
    time_audio_data_sent = 0;
    acc_num_missed_samples = 0;
    samples_ready = 0;
}

/* ============================================================================
 * A2DP events
 * ============================================================================
 */
static void a2dp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    (void)channel;
    (void)size;
    if (packet_type != HCI_EVENT_PACKET)
        return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_A2DP_META)
        return;

    switch (hci_event_a2dp_meta_get_subevent_code(packet))
    {
    case A2DP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED:
    {
        uint8_t status = a2dp_subevent_signaling_connection_established_get_status(packet);
        if (status != ERROR_CODE_SUCCESS)
        {
            spk_last_status = status;
            a2dp_cid = 0;
            spk_state = SPK_NONE;
            break;
        }
        a2dp_cid = a2dp_subevent_signaling_connection_established_get_a2dp_cid(packet);
        a2dp_subevent_signaling_connection_established_get_bd_addr(packet, speaker_addr);
        /* A paired speaker can connect to us by itself (the A2DP service
           makes us connectable); track it like one we connected to. */
        if (spk_state == SPK_NONE)
            spk_state = SPK_CONNECTING;
        break;
    }

    case A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CONFIGURATION:
    {
        if (a2dp_subevent_signaling_media_codec_sbc_configuration_get_a2dp_cid(packet) != a2dp_cid)
            break;
        uint32_t rate = a2dp_subevent_signaling_media_codec_sbc_configuration_get_sampling_frequency(packet);
        uint8_t blocks = a2dp_subevent_signaling_media_codec_sbc_configuration_get_block_length(packet);
        uint8_t subbands = a2dp_subevent_signaling_media_codec_sbc_configuration_get_subbands(packet);
        uint8_t bitpool = a2dp_subevent_signaling_media_codec_sbc_configuration_get_max_bitpool_value(packet);
        uint8_t alloc = a2dp_subevent_signaling_media_codec_sbc_configuration_get_allocation_method(packet);
        avdtp_channel_mode_t mode = (avdtp_channel_mode_t)
            a2dp_subevent_signaling_media_codec_sbc_configuration_get_channel_mode(packet);
        btstack_sbc_channel_mode_t sbc_mode;
        switch (mode)
        {
        case AVDTP_CHANNEL_MODE_JOINT_STEREO:
            sbc_mode = SBC_CHANNEL_MODE_JOINT_STEREO;
            break;
        case AVDTP_CHANNEL_MODE_STEREO:
            sbc_mode = SBC_CHANNEL_MODE_STEREO;
            break;
        case AVDTP_CHANNEL_MODE_DUAL_CHANNEL:
            sbc_mode = SBC_CHANNEL_MODE_DUAL_CHANNEL;
            break;
        default: /* mono is never offered */
            sbc_mode = SBC_CHANNEL_MODE_MONO;
            break;
        }
        cfg_rate = rate;
        cfg_channel_mode = sbc_mode;
        cfg_bitpool = bitpool;
        cfg_blocks = blocks;
        cfg_subbands = subbands;
        sbc_encoder = btstack_sbc_encoder_bluedroid_init_instance(&sbc_encoder_state);
        /* The spec's allocation method counts from 1; btstack's from 0. */
        sbc_encoder->configure(&sbc_encoder_state, SBC_MODE_STANDARD, blocks, subbands,
                               (btstack_sbc_allocation_method_t)(alloc - 1), rate, bitpool, sbc_mode);
        break;
    }

    case A2DP_SUBEVENT_STREAM_ESTABLISHED:
    {
        uint8_t status = a2dp_subevent_stream_established_get_status(packet);
        if (status != ERROR_CODE_SUCCESS)
        {
            /* The signalling channel may still be up; drop it so the state
               is simply "not connected". */
            spk_last_status = status;
            a2dp_source_disconnect(a2dp_cid);
            break;
        }
        local_seid = a2dp_subevent_stream_established_get_local_seid(packet);
        spk_state = SPK_OPEN;
        if (!spk_pairing_allowed && !CurrentLinePtr)
            MMPrintString("Bluetooth speaker connected\r\n> "); /* it reconnected by itself */
        break;
    }

    case A2DP_SUBEVENT_STREAM_STARTED:
        lead_in_samples = cfg_rate * BTA_LEAD_IN_MS / 1000;
        audio_timer_start();
        spk_state = SPK_STREAMING;
        bt_keyboard_scan_duty_changed();
        break;

    case A2DP_SUBEVENT_STREAMING_CAN_SEND_MEDIA_PACKET_NOW:
        if (spk_state == SPK_STREAMING && sbc_ready_to_send)
            send_media_packet();
        break;

    case A2DP_SUBEVENT_STREAM_SUSPENDED:
        audio_timer_stop();
        if (spk_state == SPK_STREAMING)
            spk_state = SPK_OPEN;
        bt_keyboard_scan_duty_changed();
        break;

    case A2DP_SUBEVENT_STREAM_RELEASED:
        audio_timer_stop();
        if (spk_state == SPK_STREAMING)
            spk_state = SPK_OPEN;
        bt_keyboard_scan_duty_changed();
        break;

    case A2DP_SUBEVENT_SIGNALING_CONNECTION_RELEASED:
    {
        bool was_connected = (spk_state == SPK_OPEN || spk_state == SPK_STREAMING);
        audio_timer_stop();
        a2dp_cid = 0;
        spk_state = SPK_NONE;
        bt_keyboard_scan_duty_changed();
        if (was_connected && !spk_quiet_release && !CurrentLinePtr)
            MMPrintString("Bluetooth speaker disconnected\r\n> ");
        break;
    }

    default:
        break;
    }
}

/* ============================================================================
 * Classic GAP events: inquiry, names, pairing
 * ============================================================================
 */
static bta_device_t *found_lookup(const bd_addr_t addr)
{
    for (int i = 0; i < found_count; i++)
        if (memcmp(found[i].addr, addr, sizeof(bd_addr_t)) == 0)
            return &found[i];
    return NULL;
}

static void hci_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    (void)channel;
    (void)size;
    if (packet_type != HCI_EVENT_PACKET)
        return;
    bd_addr_t addr;

    switch (hci_event_packet_get_type(packet))
    {
    /* Pairing is accepted only while BLUETOOTH CONNECT is running: we are
       connectable (for paired speakers to reconnect), so without this any
       device that knew our address could pair through Just Works. */
    case HCI_EVENT_PIN_CODE_REQUEST:
        /* Legacy (pre-2.1) pairing: speakers that still use it take "0000". */
        hci_event_pin_code_request_get_bd_addr(packet, addr);
        if (spk_pairing_allowed)
            gap_pin_code_response(addr, "0000");
        else
            gap_pin_code_negative(addr);
        break;

    case HCI_EVENT_USER_CONFIRMATION_REQUEST:
        /* Secure Simple Pairing, Just Works (neither side has a display). */
        hci_event_user_confirmation_request_get_bd_addr(packet, addr);
        if (spk_pairing_allowed)
            gap_ssp_confirmation_response(addr);
        else
            gap_ssp_confirmation_negative(addr);
        break;

    case GAP_EVENT_INQUIRY_RESULT:
    {
        gap_event_inquiry_result_get_bd_addr(packet, addr);
        bta_device_t *d = found_lookup(addr);
        if (d == NULL)
        {
            if (found_count >= BTA_MAX_FOUND)
                break;
            d = &found[found_count];
            memset(d, 0, sizeof(*d));
            memcpy(d->addr, addr, sizeof(bd_addr_t));
            d->rssi = -127;
            found_count++;
        }
        d->cod = gap_event_inquiry_result_get_class_of_device(packet);
        d->psrm = gap_event_inquiry_result_get_page_scan_repetition_mode(packet);
        d->clock_offset = gap_event_inquiry_result_get_clock_offset(packet);
        if (gap_event_inquiry_result_get_rssi_available(packet))
            d->rssi = (int8_t)gap_event_inquiry_result_get_rssi(packet);
        if (gap_event_inquiry_result_get_name_available(packet))
        {
            int len = gap_event_inquiry_result_get_name_len(packet);
            if (len > BTA_NAME_LEN - 1)
                len = BTA_NAME_LEN - 1;
            memcpy(d->name, gap_event_inquiry_result_get_name(packet), len);
            d->name[len] = 0;
        }
        break;
    }

    case GAP_EVENT_INQUIRY_COMPLETE:
        inquiry_active = false;
        break;

    case HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE:
    {
        hci_event_remote_name_request_complete_get_bd_addr(packet, addr);
        bta_device_t *d = found_lookup(addr);
        if (d && hci_event_remote_name_request_complete_get_status(packet) == ERROR_CODE_SUCCESS)
        {
            strncpy(d->name, hci_event_remote_name_request_complete_get_remote_name(packet),
                    BTA_NAME_LEN - 1);
            d->name[BTA_NAME_LEN - 1] = 0;
        }
        name_request_active = false;
        break;
    }

    default:
        break;
    }
}

/* ============================================================================
 * PLAY output service (main loop)
 * ============================================================================
 */
#define BTA_IDLE_SUSPEND_MS 5000  /* suspend the stream after this much silence */
#define BTA_START_TIMEOUT_MS 3000 /* give up waiting for a stream to start */
#define BTA_DISCARD_RATE 44100

/* Called from bt_keyboard_poll() in the main loop. With OPTION AUDIO
   BLUETOOTH it starts the stream when PLAY has something to play, and
   suspends it after BTA_IDLE_SUSPEND_MS of silence. While no speaker can
   take the sound it is discarded at the real-time rate, so PLAY behaves as
   a wired output does with nothing plugged in: sounds still end on time and
   their interrupts still fire. */
void bt_audio_service(void)
{
    static uint32_t last_us, last_active_ms, start_requested_ms;
    static bool start_requested;
    if (!bta_ready || !AUDIO_BLUETOOTH)
        return;
    uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    bool playing = bt_audio_playing();
    bool tone = (tone_target != 0 || tone_gain != 0);
    if (playing || tone)
        last_active_ms = now_ms;

    switch (spk_state)
    {
    case SPK_STREAMING:
        start_requested = false;
        last_us = time_us_32();
        if (!playing && !tone && now_ms - last_active_ms > BTA_IDLE_SUSPEND_MS)
        {
            bta_lock();
            a2dp_source_pause_stream(a2dp_cid, local_seid);
            bta_unlock();
            last_active_ms = now_ms; /* don't repeat it while the suspend completes */
        }
        return;
    case SPK_OPEN:
        if (!playing)
        {
            start_requested = false;
            break;
        }
        if (!start_requested)
        {
            bta_lock();
            uint8_t status = a2dp_source_start_stream(a2dp_cid, local_seid);
            bta_unlock();
            start_requested = (status == ERROR_CODE_SUCCESS);
            start_requested_ms = now_ms;
        }
        if (start_requested && now_ms - start_requested_ms < BTA_START_TIMEOUT_MS)
        {
            last_us = time_us_32(); /* hold the sound until the stream runs */
            return;
        }
        break;
    default:
        start_requested = false;
        break;
    }

    /* No speaker is taking the sound: discard it in real time. */
    uint32_t now_us = time_us_32();
    if (!playing)
    {
        last_us = now_us;
        return;
    }
    uint32_t frames = (uint32_t)(((uint64_t)(now_us - last_us) * BTA_DISCARD_RATE) / 1000000u);
    if (frames == 0)
        return;
    last_us += (uint32_t)(((uint64_t)frames * 1000000u) / BTA_DISCARD_RATE);
    if (frames > BTA_DISCARD_RATE / 4)
        frames = BTA_DISCARD_RATE / 4;
    while (frames)
    {
        int n = frames > 128 ? 128 : (int)frames;
        bta_lock(); /* the encoder tick may be pulling as the stream starts */
        bt_audio_pull(pcm_frame, n, BTA_DISCARD_RATE);
        bta_unlock();
        frames -= n;
    }
}

/* ============================================================================
 * Initialisation
 * ============================================================================
 */
void bt_audio_init(void)
{
    /* Ask to be master of the speaker link: the controller then schedules
       both links (speaker and keyboard) from our side. */
    hci_set_master_slave_policy(0);
    hci_set_inquiry_mode(INQUIRY_MODE_RSSI_AND_EIR);
    gap_set_local_name(CYW43_HOST_NAME);
    /* Major service class Audio, major device class Computer (desktop). */
    gap_set_class_of_device(0x200104);
    /* Speakers have no display or keys: Secure Simple Pairing, Just Works,
       confirmed in hci_packet_handler only while BLUETOOTH CONNECT runs. */
    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    gap_ssp_set_auto_accept(0);

    sdp_init();
    a2dp_source_init();
    a2dp_source_register_packet_handler(&a2dp_packet_handler);

    avdtp_stream_endpoint_t *ep = a2dp_source_create_stream_endpoint(
        AVDTP_AUDIO, AVDTP_CODEC_SBC,
        (uint8_t *)media_sbc_codec_capabilities, sizeof(media_sbc_codec_capabilities),
        media_sbc_codec_configuration, sizeof(media_sbc_codec_configuration));
    if (ep == NULL)
        return; /* btstack pools too small - BLUETOOTH reports "not available" */
    avdtp_set_preferred_sampling_frequency(ep, BTA_PREFERRED_RATE);
    local_seid = avdtp_local_seid(ep);

    memset(sdp_a2dp_source_service_buffer, 0, sizeof(sdp_a2dp_source_service_buffer));
    a2dp_source_create_sdp_record(sdp_a2dp_source_service_buffer, sdp_create_service_record_handle(),
                                  AVDTP_SOURCE_FEATURE_MASK_PLAYER, NULL, NULL);
    sdp_register_service(sdp_a2dp_source_service_buffer);

    hci_event_callback_registration.callback = &hci_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    spk_state = SPK_NONE;
    bta_ready = true;
}

/* ============================================================================
 * BLUETOOTH command
 * ============================================================================
 */
static void bta_require_ready(void)
{
    if (!bta_ready || hci_get_state() != HCI_STATE_WORKING)
        error("Bluetooth not available");
}

static void print_padded(const char *s, int width)
{
    MMPrintString((char *)s);
    for (int n = (int)strlen(s); n < width; n++)
        MMputchar(' ', 1);
}

/* BLUETOOTH SCAN [seconds] */
static void bta_scan(unsigned char *tp)
{
    int seconds = 10;
    if (*tp)
        seconds = getint(tp, 2, 60);
    bta_require_ready();
    if (inquiry_active || name_request_active)
        error("Scan already running");

    found_count = 0;
    inquiry_active = true;
    bta_lock();
    int rc = gap_inquiry_start((uint8_t)((seconds * 100 + 127) / 128)); /* units of 1.28 s */
    bta_unlock();
    if (rc)
    {
        inquiry_active = false;
        error("Scan failed to start (%)", rc);
    }
    MMPrintString("Scanning...\r\n");
    if (!bta_wait(cond_inquiry_done, (uint32_t)seconds * 1000u + 5000u))
    {
        bta_lock();
        gap_inquiry_stop();
        bta_unlock();
        inquiry_active = false;
    }

    /* Ask for the names that weren't in the extended inquiry response. */
    for (int i = 0; i < found_count; i++)
    {
        if (found[i].name[0])
            continue;
        name_request_active = true;
        bta_lock();
        rc = gap_remote_name_request(found[i].addr, found[i].psrm, found[i].clock_offset | 0x8000);
        bta_unlock();
        if (rc)
        {
            name_request_active = false;
            continue;
        }
        bta_wait(cond_name_done, 6000);
        name_request_active = false;
    }

    for (int i = 0; i < found_count; i++)
    {
        char buf[16];
        print_padded(bd_addr_to_str(found[i].addr), 19);
        sprintf(buf, "%d dBm", found[i].rssi);
        print_padded(buf, 9);
        print_padded(cod_is_audio(found[i].cod) ? "Audio" : "", 7);
        if (found[i].name[0])
        {
            MMputchar('"', 1);
            MMPrintString(found[i].name);
            MMputchar('"', 1);
        }
        PRet();
    }
    PInt(found_count);
    MMPrintString(found_count == 1 ? " device found\r\n" : " devices found\r\n");
}

/* error() with a btstack status code: "<what> (Bluetooth error 0x0C)".
   ('&' is error()'s float placeholder, so the hex is formatted here.) */
static void bta_error_status(const char *what, uint8_t status)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%s (Bluetooth error 0x%02X)", what, status);
    error("$", buf);
}

/* BLUETOOTH CONNECT addr$ */
static void bta_connect(unsigned char *tp)
{
    bd_addr_t addr;
    if (!*tp)
        SyntaxError();
    char *s = (char *)getCstring(tp);
    bta_require_ready();
    if (sscanf_bd_addr(s, addr) == 0)
        error("Invalid Bluetooth address");
    if (inquiry_active || name_request_active)
        error("Scan in progress");

    /* Checked and started under the lock: a paired speaker may be
       connecting to us by itself at the same moment. */
    uint8_t status;
    bool busy;
    bta_lock();
    busy = (spk_state != SPK_NONE);
    if (!busy)
    {
        status = a2dp_source_establish_stream(addr, &a2dp_cid);
        if (status == ERROR_CODE_SUCCESS)
        {
            spk_last_status = 0;
            spk_pairing_allowed = true;
            spk_state = SPK_CONNECTING;
        }
    }
    bta_unlock();
    if (busy)
        error(spk_state == SPK_CONNECTING ? "Speaker already connecting" : "Speaker already connected");
    if (status != ERROR_CODE_SUCCESS)
        bta_error_status("Connect failed", status);

    bool answered = bta_wait(cond_not_connecting, 30000);
    spk_pairing_allowed = false;
    if (!answered)
    {
        spk_quiet_release = true;
        bta_lock();
        a2dp_source_disconnect(a2dp_cid);
        bta_unlock();
        bta_wait(cond_released, 3000);
        spk_quiet_release = false;
        spk_state = SPK_NONE;
        error("Speaker did not answer");
    }
    if (spk_state != SPK_OPEN)
        bta_error_status("Connection failed", spk_last_status);

    MMPrintString("Connected to ");
    MMPrintString((char *)bd_addr_to_str(speaker_addr));
    MMPrintString(": SBC ");
    PInt(cfg_rate);
    MMPrintString(" Hz, ");
    MMPrintString(cfg_channel_mode == SBC_CHANNEL_MODE_JOINT_STEREO ? "joint stereo" : (cfg_channel_mode == SBC_CHANNEL_MODE_STEREO ? "stereo" : "dual channel"));
    MMPrintString(", bitpool ");
    PInt(cfg_bitpool);
    PRet();
}

/* BLUETOOTH DISCONNECT */
static void bta_disconnect(void)
{
    bta_require_ready();
    if (spk_state == SPK_NONE)
        error("No speaker connected");
    spk_quiet_release = true;
    bta_lock();
    a2dp_source_disconnect(a2dp_cid);
    bta_unlock();
    bool ok = bta_wait(cond_released, 5000);
    spk_quiet_release = false;
    if (!ok)
        error("Speaker did not release the connection");
}

static bool cond_tone_silent(void) { return tone_gain == 0; }
static bool cond_never(void) { return false; }

/* BLUETOOTH TEST freq  - stream a sine tone of freq Hz (fades in; starts the
                          stream if it is suspended)
   BLUETOOTH TEST 0     - fade the tone out; the stream keeps running silent
   BLUETOOTH TEST STOP  - fade out, send 200 ms of silence, suspend the stream */
static void bta_test(unsigned char *tp)
{
    bool stop = false;
    int freq = 0;
    if (checkstring(tp, (unsigned char *)"STOP"))
        stop = true;
    else
        freq = getint(tp, 0, 20000);
    bta_require_ready();
    if (spk_state != SPK_OPEN && spk_state != SPK_STREAMING)
        error("No speaker connected");
    if (!sine_ready)
    {
        for (int i = 0; i < 256; i++)
            sine_table[i] = (int16_t)(8000.0f * sinf((float)i * (2.0f * 3.14159265f / 256.0f)));
        sine_ready = true;
    }
    if (stop)
    {
        tone_target = 0;
        if (spk_state == SPK_STREAMING)
        {
            bta_wait(cond_tone_silent, 500);
            bta_wait(cond_never, 200);
            bta_lock();
            a2dp_source_pause_stream(a2dp_cid, local_seid);
            bta_unlock();
            bta_wait(cond_not_streaming, 5000);
        }
        return;
    }
    if (freq)
    {
        if (tone_gain == 0)
            tone_phase = 0; /* start from a zero crossing */
        tone_step = (uint32_t)(((uint64_t)freq << 32) / cfg_rate);
        __compiler_memory_barrier();
        tone_target = BTA_GAIN_FULL;
        if (spk_state == SPK_OPEN)
        {
            bta_lock();
            uint8_t status = a2dp_source_start_stream(a2dp_cid, local_seid);
            bta_unlock();
            if (status != ERROR_CODE_SUCCESS)
                bta_error_status("Stream start failed", status);
            if (!bta_wait(cond_streaming, 5000) || spk_state != SPK_STREAMING)
                error("Speaker did not start the stream");
        }
    }
    else
        tone_target = 0;
}

/* BLUETOOTH STATUS */
static void bta_status(void)
{
    MMPrintString("Bluetooth: ");
    MMPrintString(bta_ready && hci_get_state() == HCI_STATE_WORKING ? "on" : "not available");
    MMPrintString("\r\nKeyboard:  ");
    MMPrintString(bt_keyboard_ready() ? "connected" : "not connected");
    MMPrintString("\r\nSpeaker:   ");
    switch (spk_state)
    {
    case SPK_NONE:
        MMPrintString("not connected");
        break;
    case SPK_CONNECTING:
        MMPrintString("connecting");
        break;
    case SPK_OPEN:
    case SPK_STREAMING:
        MMPrintString((char *)bd_addr_to_str(speaker_addr));
        MMPrintString(spk_state == SPK_STREAMING ? ", streaming" : ", idle");
        MMPrintString("\r\nStream:    SBC ");
        PInt(cfg_rate);
        MMPrintString(" Hz, ");
        MMPrintString(cfg_channel_mode == SBC_CHANNEL_MODE_JOINT_STEREO ? "joint stereo" : (cfg_channel_mode == SBC_CHANNEL_MODE_STEREO ? "stereo" : "dual channel"));
        MMPrintString(", bitpool ");
        PInt(cfg_bitpool);
        MMPrintString(", ");
        PInt(cfg_blocks);
        MMPrintString(" blocks, ");
        PInt(cfg_subbands);
        MMPrintString(" subbands");
        break;
    }
    if (spk_state == SPK_STREAMING)
    {
        uint32_t streamed_us = time_us_32() - load_start_us;
        uint32_t busy = load_busy_us;
        uint32_t permille = streamed_us ? (uint32_t)(((uint64_t)busy * 1000u) / streamed_us) : 0;
        char buf[64];
        sprintf(buf, "\r\nEncoder:   %lu.%lu%% of CPU, %lu packets in %lu s",
                (unsigned long)(permille / 10), (unsigned long)(permille % 10),
                (unsigned long)packets_sent, (unsigned long)(streamed_us / 1000000u));
        MMPrintString(buf);
    }
    if (AUDIO_BLUETOOTH)
    {
        /* PLAY samples the encoder wanted but the swing buffers hadn't got;
           counted since the last STATUS. */
        char buf[48];
        sprintf(buf, "\r\nUnderruns: %lu", (unsigned long)bt_audio_underruns);
        bt_audio_underruns = 0;
        MMPrintString(buf);
    }
    {
        /* Diagnostics, counted since the last STATUS: the longest stall of
           the encoder tick (the btstack context blocked), the bond-store
           writes (each one a SaveOptions()), and keyboard security events. */
        uint32_t saves, save_max, save_total, pairings, reenc;
        char buf[96];
        bt_keyboard_stats(&saves, &save_max, &save_total, &pairings, &reenc);
        sprintf(buf, "\r\nLongest encoder gap: %lu ms", (unsigned long)(tick_gap_max_us / 1000));
        tick_gap_max_us = 0;
        MMPrintString(buf);
        sprintf(buf, "\r\nLongest send wait: %lu ms, samples dropped: %lu",
                (unsigned long)(send_wait_max_us / 1000), (unsigned long)samples_dropped);
        send_wait_max_us = samples_dropped = 0;
        MMPrintString(buf);
        sprintf(buf, "\r\nBond store: %lu writes, longest %lu ms, total %lu ms",
                (unsigned long)saves, (unsigned long)(save_max / 1000), (unsigned long)(save_total / 1000));
        MMPrintString(buf);
        sprintf(buf, "\r\nKeyboard security: %lu pairings, %lu re-encryptions since boot",
                (unsigned long)pairings, (unsigned long)reenc);
        MMPrintString(buf);
    }
    PRet();
}

void cmd_bluetooth(void)
{
    unsigned char *tp;
    if ((tp = checkstring(cmdline, (unsigned char *)"SCAN")))
        bta_scan(tp);
    else if ((tp = checkstring(cmdline, (unsigned char *)"CONNECT")))
        bta_connect(tp);
    else if ((tp = checkstring(cmdline, (unsigned char *)"DISCONNECT")))
        bta_disconnect();
    else if ((tp = checkstring(cmdline, (unsigned char *)"TEST")))
        bta_test(tp);
    else if ((tp = checkstring(cmdline, (unsigned char *)"STATUS")))
        bta_status();
    else
        SyntaxError();
}

#endif /* PICOMITEBTH || PICOMITEHDMIBTH */
