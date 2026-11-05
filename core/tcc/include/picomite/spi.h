/* Native SPI master (API 7); see docs/PicoMite-native-C-support.md. */
#ifndef _PICOMITE_SPI_H
#define _PICOMITE_SPI_H

typedef int pm_spi_t;

pm_spi_t pm_spi_open(int bus, unsigned speed_hz, unsigned mode, unsigned bits);
void pm_spi_close(pm_spi_t bus);
int pm_spi_transfer(pm_spi_t bus, const void *tx, void *rx, unsigned count);
int pm_spi_write(pm_spi_t bus, const void *tx, unsigned count);
int pm_spi_read(pm_spi_t bus, void *rx, unsigned count);

#endif
