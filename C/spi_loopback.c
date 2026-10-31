/* SPI0 loopback: connect MOSI (TX) to MISO (RX) with a wire.
   Assign SPI0 TX/RX/SCK with BASIC SETPIN first. */
#include <picomite/spi.h>
#include <stdio.h>

int main(void)
{
    unsigned char tx[] = {0x00, 0x55, 0xaa, 0xff, 0x81};
    unsigned char rx[sizeof(tx)] = {0};
    pm_spi_t bus = pm_spi_open(0, 1000000, 0, 8);
    int n = pm_spi_transfer(bus, tx, rx, sizeof(tx));
    pm_spi_close(bus);
    for (unsigned i = 0; i < sizeof(tx); ++i)
        printf("%02X -> %02X%s\n", tx[i], rx[i], tx[i] == rx[i] ? "" : " MISMATCH");
    return n != sizeof(tx);
}
