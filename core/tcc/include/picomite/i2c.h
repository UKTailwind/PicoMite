/* Native I2C master (API 6); see docs/PicoMite-native-C-support.md. */
#ifndef _PICOMITE_I2C_H
#define _PICOMITE_I2C_H

typedef int pm_i2c_t;

pm_i2c_t pm_i2c_open(int bus, unsigned speed_khz, unsigned timeout_ms);
void pm_i2c_close(pm_i2c_t bus);
int pm_i2c_transfer(pm_i2c_t bus, unsigned address, const void *tx,
                    unsigned txlen, void *rx, unsigned rxlen);
int pm_i2c_write(pm_i2c_t bus, unsigned address, const void *tx, unsigned len);
int pm_i2c_read(pm_i2c_t bus, unsigned address, void *rx, unsigned len);
int pm_i2c_probe(pm_i2c_t bus, unsigned address);

#endif
