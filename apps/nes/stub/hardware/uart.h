/* hardware/uart.h: frank-nes's printf goes to the CMM2's serial console through the loader's services, with nothing to set up */
#include "pico/stdlib.h"
#define uart0 0
#define uart1 0
#define GPIO_FUNC_UART 0
#define uart_init(u, b) do { } while(0)
#define uart_set_format(u, a, b, c) do { } while(0)
#define uart_set_fifo_enabled(u, e) do { } while(0)
#define uart_set_irqs_enabled(u, a, b) do { } while(0)
#define gpio_set_function(p, f) do { } while(0)
