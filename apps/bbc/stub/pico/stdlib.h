/* pico/stdlib.h: what frank-micro asks of the Pico SDK, for the CMM2: the time, and the RP2350's clock, GPIO and watchdog, which here do nothing. The other headers of stub/
   include this one. */
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

void sleep_ms(uint32_t ms);
void sleep_us(uint64_t us);
uint64_t time_us_64(void);
void tight_loop_contents(void);                 /* (bbc_cmm2.c) */
int getchar_timeout_us(uint32_t timeout_us);    /* (frank_console.c's: not built) */
#define PICO_ERROR_TIMEOUT (-1)

#define GPIO_IN 0
#define gpio_init(p) do { } while(0)
#define gpio_set_dir(p, d) do { } while(0)
#define gpio_pull_down(p) do { } while(0)
#define clk_sys 0
#define clock_get_hz(c) 480000000u

static struct { uint32_t scratch[4], ctrl; } cmm2_watchdog;   /* (the serial console's STATUS) */
#define watchdog_hw (&cmm2_watchdog)


/* the RP2350's sections and attributes: the functions frank-micro keeps in RAM go in the program's SRAM part (the
   PicoMite's app.ld .sram: copied into the SRAM heap for the run), as they are its hot ones; the rest nothing */
#define PM_SRAM_STR2(x) #x
#define PM_SRAM_STR(x) PM_SRAM_STR2(x)
#define PM_SRAM_SECTION __attribute__((section(".sram.f" PM_SRAM_STR(__COUNTER__))))   /* (a section each, by a count:
                                                  frank-micro writes some names with attributes, __used f) */
#ifdef BBC_SRAM_FRANK                            /* (make.py sram=frank) */
#define __time_critical_func(f) PM_SRAM_SECTION f
#define __not_in_flash_func(f) PM_SRAM_SECTION f
#define __no_inline_not_in_flash_func(f) __attribute__((noinline)) PM_SRAM_SECTION f
#else
#define __time_critical_func(f) f
#define __not_in_flash_func(f) f
#define __no_inline_not_in_flash_func(f) __attribute__((noinline)) f
#endif
#define __scratch_x(n)
#define __scratch_y(n)
#define __not_in_flash(n)
#define __in_flash(n)
#ifndef __unused
#define __unused __attribute__((unused))
#endif
#ifndef __force_inline
#define __force_inline inline __attribute__((always_inline))
#endif
#ifndef count_of
#define count_of(a) (sizeof(a) / sizeof((a)[0]))
#endif
#define __compiler_memory_barrier() __asm volatile("" ::: "memory")
#define hw_clear_bits(p, m) do { } while(0)
#define hw_set_bits(p, m) do { } while(0)
static struct { uint32_t ctrl; } cmm2_xip_ctrl;
#define xip_ctrl_hw (&cmm2_xip_ctrl)
#define XIP_CTRL_ERR_BADWRITE_BITS 0
void watchdog_reboot(uint32_t pc, uint32_t sp, uint32_t delay_ms);   /* (bbc_cmm2.c: not called, frank_settings.c patched) */
void panic(const char *fmt, ...);
typedef uint64_t absolute_time_t;
static inline absolute_time_t get_absolute_time(void) { return time_us_64(); }
static inline uint64_t to_us_since_boot(absolute_time_t t) { return t; }
static inline absolute_time_t from_us_since_boot(uint64_t us) { return us; }
static inline void busy_wait_until(absolute_time_t t) { while(time_us_64() < t) { } }
static inline void busy_wait_us(uint64_t us) { busy_wait_until(time_us_64() + us); }
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#define __fast_mul(a, b) ((a) * (b))
/* the Pico SDK's debug pins, clock and voltage: nothing here */
#define CU_REGISTER_DEBUG_PINS(...)
#define CU_SELECT_DEBUG_PINS(...)
#define DEBUG_PINS_SET(p, v) do { } while(0)
#define DEBUG_PINS_CLR(p, v) do { } while(0)
#define DEBUG_PINS_XOR(p, v) do { } while(0)
#define hard_assert(x) ((void)0)
#define VREG_VOLTAGE_1_25 0
#define vreg_set_voltage(v) do { } while(0)
#define set_sys_clock_khz(k, b) true
/* where the RP2350's empty ROM slots read from (its flash, uncached): 16 KB of &FF here (bbc_cmm2.c) */
extern uint8_t cmm2_garbage[];
#define XIP_NOCACHE_NOALLOC_BASE cmm2_garbage
/* the serial port: its characters come by the console (bbc_cmm2.c) */
typedef struct uart_inst uart_inst_t;
#define uart_default ((uart_inst_t *)0)
#define uart_is_readable(u) 0
#define uart_is_readable_within_us(u, t) 0
#define uart_getc(u) 0
/* one core: locks and semaphores have nothing to wait for */
typedef struct mutex { int x; } mutex_t;
typedef struct semaphore { int x; } semaphore_t;
#define mutex_init(m) do { } while(0)
#define mutex_enter_blocking(m) do { } while(0)
#define mutex_exit(m) do { } while(0)
#define sem_init(s, a, b) do { } while(0)
#define sem_release(s) do { } while(0)
#define sem_acquire_blocking(s) do { } while(0)

#ifdef __cplusplus
}
#endif
#endif
