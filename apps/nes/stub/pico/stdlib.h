/* pico/stdlib.h: what frank-nes asks of the Pico SDK, for the CMM2: the time, the memory PSRAM was, and the RP2350's clock, voltage,
   LED and fault registers, which here do nothing. The other headers of stub/ include this one. */
#ifndef CMM2_PICO_STDLIB_H
#define CMM2_PICO_STDLIB_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int uint;

extern uint8_t *cmm2_psram;                     /* the block that stands for the 8 MB of PSRAM (patches/) */

void sleep_ms(uint32_t ms);
void sleep_us(uint64_t us);
uint64_t time_us_64(void);
static inline uint32_t time_us_32(void) { return (uint32_t)time_us_64(); }
typedef uint64_t absolute_time_t;
static inline absolute_time_t get_absolute_time(void) { return time_us_64(); }
static inline uint32_t to_ms_since_boot(absolute_time_t t) { return (uint32_t)(t / 1000); }
static inline int64_t absolute_time_diff_us(absolute_time_t from, absolute_time_t to) { return (int64_t)(to - from); }

#define __not_in_flash(x)
#define __not_in_flash_func(f) f
#define __no_inline_not_in_flash_func(f) f
#define __time_critical_func(f) f
#define __compiler_memory_barrier() __asm volatile("" ::: "memory")
#define tight_loop_contents() do { } while(0)
#define __wfe() do { } while(0)
#define __wfi() do { } while(0)

#define PICO_DEFAULT_LED_PIN 25
#define GPIO_OUT 1
#define gpio_init(p) do { } while(0)
#define gpio_set_dir(p, d) do { } while(0)
#define gpio_put(p, v) do { } while(0)
#define vreg_disable_voltage_limit() do { } while(0)
#define vreg_set_voltage(v) do { } while(0)
#define set_sys_clock_khz(k, r) do { } while(0)
#define stdio_init_all() do { } while(0)
#define clk_sys 0
#define clock_get_hz(c) 480000000u

#define xip_cache_clean_all() do { } while(0)
#define xip_cache_invalidate_all() do { } while(0)

/* set_flash_timings and the fault report are built as they are, and not run to any effect */
static struct { struct { uint32_t timing; } m[1]; } cmm2_qmi;
#define qmi_hw (&cmm2_qmi)
#define QMI_M0_TIMING_RXDELAY_LSB 8
#define QMI_M0_TIMING_CLKDIV_LSB 0
static struct { uint32_t scratch[8]; } cmm2_watchdog;
#define watchdog_hw (&cmm2_watchdog)

#ifdef __cplusplus
}
#endif
#endif
