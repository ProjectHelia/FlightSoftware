/**
 * @file hal_internal.h
 * @brief Shared plumbing between the Instrumentation HAL's per-sensor
 *        source files - not part of the public hal.h API. Each sensor
 *        (hal_sht45.c, hal_ads1115.c, hal_scd41.c, hal_ms5611.c,
 *        hal_imu.c) implements its own hal_read_<sensor>() from hal.h
 *        plus a <sensor>_probe() declared here, called once from
 *        hal_esp32.c's hal_init().
 */
#ifndef HELIA_INSTRUMENTATION_HAL_INTERNAL_H
#define HELIA_INSTRUMENTATION_HAL_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief I2C port shared by every sensor driver and the bus setup in
 *         hal_esp32.c. Whoever uses this needs driver/i2c.h included too
 *         (for I2C_NUM_0) - this header intentionally doesn't pull it in,
 *         so sensor files that never touch the port directly don't need
 *         the ESP-IDF I2C driver headers at all. */
#define I2C_PORT I2C_NUM_0

/* ---- i2c_bus.c: shared low-level I2C master transaction helpers ---- */
bool i2c_probe(uint8_t addr);
bool i2c_write(uint8_t addr, const uint8_t *data, size_t len);
bool i2c_read(uint8_t addr, uint8_t *data, size_t len);
bool i2c_write_read(uint8_t addr, const uint8_t *wr, size_t wr_len, uint8_t *rd, size_t rd_len);

/** @brief Sensirion CRC-8 (poly 0x31, init 0xFF) - shared by SHT45 and SCD41. */
uint8_t sensirion_crc8(const uint8_t *data, size_t len);

/* ---- per-sensor probe/init, called once from hal_esp32.c's hal_init() ---- */
bool hal_sht45_probe(void);
bool hal_ads1115_probe(void);
bool hal_scd41_probe(void);
bool hal_ms5611_probe(void);
bool hal_imu_probe(void);

/** @brief Which of the two possible addresses MS5611 probed OK at (0x76
 *         or 0x77) - only meaningful after hal_ms5611_probe() returns
 *         true. Purely for the boot-log summary in hal_esp32.c. */
uint8_t hal_ms5611_addr(void);

#endif /* HELIA_INSTRUMENTATION_HAL_INTERNAL_H */