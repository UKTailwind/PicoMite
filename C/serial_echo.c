/* PicoMite TinyCC COM1 echo. Configure COM1 RX/TX pins with SETPIN first.
   RUN "B:/serial_echo.c"; send bytes from a terminal to COM1 at 115200 8N1.
   Ctrl+C ends the run and releases the C-owned port. */
#include <picomite/serial.h>
#include <picomite/system.h>
#include <stdio.h>

int main(void)
{
    pm_serial_config cfg = {1, 115200, 256, 0};
    unsigned char buf[64];
    pm_serial_t port = pm_serial_open(&cfg);
    puts("COM1 echo active; Ctrl+C stops it");
    for (;;)
    {
        int got = pm_serial_read(port, buf, sizeof(buf), 100);
        unsigned offset = 0;
        while (offset < (unsigned)got)
        {
            int sent = pm_serial_write(port, buf + offset, (unsigned)got - offset, 100);
            if (sent == 0)
            {
                puts("COM1 transmit timed out");
                pm_serial_close(port);
                return 1;
            }
            offset += (unsigned)sent;
        }
        pm_yield();
    }
}
