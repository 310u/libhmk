/*
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "common.h"

#if !defined(I2C_NUM_BUSES)
#define I2C_NUM_BUSES 0
#endif

#if !defined(I2C_TRANSFER_TIMEOUT_MS)
#define I2C_TRANSFER_TIMEOUT_MS 50u
#endif

// Enable I2C buses by defining the per-bus pin macros in board_def.h.
// AT32 backends expect I2C_BUSn_INSTANCE/CLOCK/SCL_PORT/SCL_PIN/
// SCL_PIN_SOURCE/SDA_PORT/SDA_PIN/SDA_PIN_SOURCE/PIN_MUX.
// STM32 backends expect I2C_BUSn_INSTANCE/CLOCK_ENABLE()/SCL_PORT/SCL_PIN/
// SDA_PORT/SDA_PIN/PIN_AF.

typedef struct {
  uint8_t bus;
  uint32_t frequency_hz;
} i2c_bus_config_t;

typedef enum {
  I2C_REGISTER_8BIT = 1,
  I2C_REGISTER_16BIT = 2,
} i2c_register_width_t;

void i2c_bus_init(void);
bool i2c_bus_acquire(const i2c_bus_config_t *config);
void i2c_bus_release(const i2c_bus_config_t *config);
bool i2c_bus_write(const i2c_bus_config_t *config, uint8_t address,
                   const uint8_t *tx, size_t len);
bool i2c_bus_read(const i2c_bus_config_t *config, uint8_t address, uint8_t *rx,
                  size_t len);
bool i2c_bus_write_register(const i2c_bus_config_t *config, uint8_t address,
                            uint16_t reg,
                            i2c_register_width_t register_width,
                            const uint8_t *tx, size_t len);
bool i2c_bus_read_register(const i2c_bus_config_t *config, uint8_t address,
                           uint16_t reg,
                           i2c_register_width_t register_width, uint8_t *rx,
                           size_t len);
