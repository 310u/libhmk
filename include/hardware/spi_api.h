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

#if !defined(SPI_NUM_BUSES)
#define SPI_NUM_BUSES 0
#endif

// Enable SPI buses by defining the per-bus pin macros in board_def.h.
// AT32 backends expect SPI_BUSn_INSTANCE/CLOCK/CLOCK_HZ/SCK_PORT/SCK_PIN/
// SCK_PIN_SOURCE/PIN_MUX. STM32 backends expect SPI_BUSn_INSTANCE/
// CLOCK_ENABLE()/CLOCK_HZ/SCK_PORT/SCK_PIN/PIN_AF. MISO and MOSI are optional:
// define *_PORT as NULL and *_PIN as 0 when they are unused.

typedef enum {
  SPI_BUS_MODE_0 = 0,
  SPI_BUS_MODE_1,
  SPI_BUS_MODE_2,
  SPI_BUS_MODE_3,
} spi_bus_mode_t;

typedef struct {
  uint8_t bus;
  uint32_t frequency_hz;
  spi_bus_mode_t mode;
  bool lsb_first;
} spi_bus_config_t;

typedef struct {
  void *port;
  uint32_t pin;
  bool active_low;
} spi_chip_select_t;

void spi_bus_init(void);
bool spi_bus_acquire(const spi_bus_config_t *config);
void spi_bus_release(const spi_bus_config_t *config);
bool spi_bus_transfer(const spi_bus_config_t *config, const uint8_t *tx,
                      uint8_t *rx, size_t len);
void spi_cs_init(const spi_chip_select_t *chip_select);
void spi_cs_select(const spi_chip_select_t *chip_select);
void spi_cs_deselect(const spi_chip_select_t *chip_select);

static inline bool spi_bus_write(const spi_bus_config_t *config,
                                 const uint8_t *tx, size_t len) {
  return spi_bus_transfer(config, tx, NULL, len);
}

static inline bool spi_bus_read(const spi_bus_config_t *config, uint8_t *rx,
                                size_t len) {
  return spi_bus_transfer(config, NULL, rx, len);
}
