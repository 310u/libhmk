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

#include "trackball.h"
#include "rgb.h"

#include <string.h>

#include "eeconfig.h"
#include "hardware/hardware.h"
#include "hid.h"
#include "layout.h"
#include "lib/usqrt.h"

#if defined(TRACKBALL_ENABLED)

#if defined(TRACKBALL_SENSOR_PMW3360)
#define TRACKBALL_SENSOR_NAME "PMW3360"
#elif defined(TRACKBALL_SENSOR_PAW3395)
#define TRACKBALL_SENSOR_NAME "PAW3395"
#elif defined(TRACKBALL_SENSOR_PAW3950)
#error "TRACKBALL_SENSOR_PAW3950 is planned but not implemented yet. Public PixArt documentation was not available when this driver was written."
#else
#error "Unsupported trackball sensor"
#endif

#if !defined(TRACKBALL_SPI_BUS)
#error "TRACKBALL_SPI_BUS not defined in board_def.h"
#endif

#if !defined(TRACKBALL_CS_PORT) || !defined(TRACKBALL_CS_PIN)
#error "TRACKBALL_CS_PORT and TRACKBALL_CS_PIN must be defined in board_def.h"
#endif

#if defined(__has_include)
#if __has_include("at32f402_405.h")
#include "at32f402_405.h"
#define TRACKBALL_GPIO_BACKEND_AT32 1
#elif __has_include("stm32f4xx_hal.h")
#include "stm32f4xx_hal.h"
#define TRACKBALL_GPIO_BACKEND_STM32 1
#endif
#endif

#if !defined(TRACKBALL_GPIO_BACKEND_AT32) && !defined(TRACKBALL_GPIO_BACKEND_STM32)
#error "Unsupported GPIO backend for trackball"
#endif

#ifndef TRACKBALL_SPI_FREQUENCY_HZ
#if defined(TRACKBALL_SENSOR_PAW3395)
#define TRACKBALL_SPI_FREQUENCY_HZ 8000000u
#else
#define TRACKBALL_SPI_FREQUENCY_HZ 2000000u
#endif
#endif

#ifndef TRACKBALL_SPI_MODE
#define TRACKBALL_SPI_MODE SPI_BUS_MODE_3
#endif

#ifndef TRACKBALL_POLL_INTERVAL_MS
#define TRACKBALL_POLL_INTERVAL_MS 1u
#endif

#ifndef TRACKBALL_RECOVERY_RETRY_MS
#define TRACKBALL_RECOVERY_RETRY_MS 250u
#endif

#ifndef TRACKBALL_MAX_CONSECUTIVE_ERRORS
#define TRACKBALL_MAX_CONSECUTIVE_ERRORS 3u
#endif

#ifndef TRACKBALL_CPI_DEFAULT
#define TRACKBALL_CPI_DEFAULT 1600u
#endif

#ifndef TRACKBALL_MOTION_ACTIVE_LOW
#define TRACKBALL_MOTION_ACTIVE_LOW 1
#endif

#define TRACKBALL_MOUSE_FP_SHIFT 8
#define TRACKBALL_MOUSE_FP_ONE (1L << TRACKBALL_MOUSE_FP_SHIFT)
#define TRACKBALL_MOUSE_DIVISOR 50L
#define TRACKBALL_VECTOR_MAX 256L

#ifndef TRACKBALL_MOTION_PULL_UP
#if defined(TRACKBALL_SENSOR_PAW3395)
#define TRACKBALL_MOTION_PULL_UP 1
#else
#define TRACKBALL_MOTION_PULL_UP 0
#endif
#endif

#ifndef TRACKBALL_SWAP_XY
#define TRACKBALL_SWAP_XY 0
#endif

#ifndef TRACKBALL_INVERT_X
#define TRACKBALL_INVERT_X 0
#endif

#ifndef TRACKBALL_INVERT_Y
#define TRACKBALL_INVERT_Y 0
#endif

#define PMW3360_PRODUCT_ID 0x42u
#define PMW3360_POWER_UP_RESET_VALUE 0x5Au
#define PMW3360_REG_PRODUCT_ID 0x00u
#define PMW3360_REG_MOTION 0x02u
#define PMW3360_REG_DELTA_X_L 0x03u
#define PMW3360_REG_DELTA_X_H 0x04u
#define PMW3360_REG_DELTA_Y_L 0x05u
#define PMW3360_REG_DELTA_Y_H 0x06u
#define PMW3360_REG_CONFIG1 0x0Fu
#define PMW3360_REG_CONFIG2 0x10u
#define PMW3360_REG_POWER_UP_RESET 0x3Au
#define PMW3360_REG_MOTION_BURST 0x50u

#define PMW3360_SPI_WRITE_BIT 0x80u
#define PMW3360_MOTION_FLAG_MOTION 0x80u
#define PMW3360_MOTION_FLAG_LIFTED 0x08u
#define PMW3360_MOTION_BURST_LENGTH 6u

#define PAW3395_PRODUCT_ID 0x51u
#define PAW3395_INV_PRODUCT_ID 0xAEu
#define PAW3395_POWER_UP_RESET_VALUE 0x5Au
#define PAW3395_REG_PRODUCT_ID 0x00u
#define PAW3395_REG_MOTION 0x02u
#define PAW3395_REG_DELTA_X_L 0x03u
#define PAW3395_REG_DELTA_X_H 0x04u
#define PAW3395_REG_DELTA_Y_L 0x05u
#define PAW3395_REG_DELTA_Y_H 0x06u
#define PAW3395_REG_OBSERVATION 0x15u
#define PAW3395_REG_MOTION_BURST 0x16u
#define PAW3395_REG_POWER_UP_RESET 0x3Au
#define PAW3395_REG_SET_RESOLUTION 0x47u
#define PAW3395_REG_RESOLUTION_X_LOW 0x48u
#define PAW3395_REG_RESOLUTION_X_HIGH 0x49u
#define PAW3395_REG_RESOLUTION_Y_LOW 0x4Au
#define PAW3395_REG_RESOLUTION_Y_HIGH 0x4Bu
#define PAW3395_REG_MOTION_CTRL 0x5Cu
#define PAW3395_REG_INV_PRODUCT_ID 0x5Fu
#define PAW3395_REG_BOOT_STATUS 0x6Cu
#define PAW3395_REG_PAGE_SELECT 0x7Fu

#define PAW3395_MOTION_FLAG_MOTION 0x80u
#define PAW3395_MOTION_FLAG_LIFTED 0x08u
#define PAW3395_MOTION_BURST_LENGTH 12u
#define PAW3395_MOTION_CTRL_UNIFIED_RESOLUTION 0x00u
#define PAW3395_BOOT_STATUS_READY 0x80u
#define PAW3395_CPI_STEP 50u
#define PAW3395_CPI_MIN 50u
#define PAW3395_CPI_MAX 26000u

typedef struct {
  bool motion;
  bool restart_burst;
  int16_t dx;
  int16_t dy;
} trackball_motion_sample_t;

typedef struct {
  uint8_t reg;
  uint8_t value;
} trackball_register_write_t;

typedef struct {
  uint8_t write_bit;
  uint32_t ncs_sclk_ns;
  uint32_t read_addr_delay_us;
  uint32_t read_post_us;
  uint32_t write_pre_deselect_us;
  uint32_t write_post_us;
  uint32_t burst_addr_delay_us;
  uint32_t burst_post_us;
  uint32_t burst_exit_ns;
} trackball_spi_protocol_t;

typedef struct {
  const char *name;
  spi_bus_mode_t spi_mode;
  const trackball_spi_protocol_t *spi;
  bool (*init)(void);
  bool (*set_cpi)(uint16_t cpi);
  bool (*start_burst)(void);
  bool (*read_motion)(trackball_motion_sample_t *sample);
} trackball_sensor_api_t;

static const trackball_sensor_api_t trackball_sensor_api;

typedef struct {
  spi_bus_config_t bus_config;
  spi_chip_select_t chip_select;
  bool enabled;
  bool burst_mode_started;
  uint8_t consecutive_errors;
  uint32_t last_poll_ms;
  uint32_t last_recovery_attempt_ms;
  uint16_t current_cpi;
  int16_t last_dx;
  int16_t last_dy;
} trackball_state_t;

static trackball_state_t trackball_state = {
    .bus_config =
        {
            .bus = TRACKBALL_SPI_BUS,
            .frequency_hz = TRACKBALL_SPI_FREQUENCY_HZ,
            .mode = TRACKBALL_SPI_MODE,
            .lsb_first = false,
        },
    .chip_select =
        {
            .port = TRACKBALL_CS_PORT,
            .pin = TRACKBALL_CS_PIN,
            .active_low = true,
        },
    .enabled = false,
    .burst_mode_started = false,
    .consecutive_errors = 0,
    .last_poll_ms = 0,
    .last_recovery_attempt_ms = 0,
    .current_cpi = TRACKBALL_CPI_DEFAULT,
    .last_dx = 0,
    .last_dy = 0,
};

static trackball_config_t trackball_config_cache = {
    .cpi = TRACKBALL_CPI_DEFAULT,
    .enabled = true,
    .invert_x = false,
    .invert_y = false,
    .swap_xy = false,
    .mouse_speed = TRACKBALL_MOUSE_SPEED_DEFAULT,
    .mouse_acceleration = TRACKBALL_MOUSE_ACCELERATION_DEFAULT,
    .active_mouse_preset = 0u,
    .smoothing = TRACKBALL_SMOOTHING_DEFAULT,
};

static int32_t trackball_mouse_accum_x = 0;
static int32_t trackball_mouse_accum_y = 0;
static int32_t trackball_filtered_dx = 0;
static int32_t trackball_filtered_dy = 0;
static uint32_t trackball_last_mouse_tick = 0;

static inline void trackball_delay_cycles(uint32_t cycles) {
  const uint32_t start = board_cycle_count();
  while ((uint32_t)(board_cycle_count() - start) < cycles) {
  }
}

static void trackball_delay_ns(uint32_t ns) {
  uint64_t cycles = ((uint64_t)F_CPU * ns) / 1000000000ull;
  if (cycles == 0u) {
    cycles = 1u;
  }

  trackball_delay_cycles((uint32_t)cycles);
}

static void trackball_delay_us(uint32_t us) {
  uint64_t cycles = ((uint64_t)F_CPU * us) / 1000000ull;
  if (cycles == 0u) {
    cycles = 1u;
  }

  trackball_delay_cycles((uint32_t)cycles);
}

#if defined(TRACKBALL_MOTION_PORT) && defined(TRACKBALL_MOTION_PIN)
static void trackball_enable_gpio_clock(void *port) {
#if defined(TRACKBALL_GPIO_BACKEND_AT32)
  if (port == NULL) {
    return;
  }
#if defined(GPIOA)
  if (port == GPIOA) {
    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
    return;
  }
#endif
#if defined(GPIOB)
  if (port == GPIOB) {
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
    return;
  }
#endif
#if defined(GPIOC)
  if (port == GPIOC) {
    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
    return;
  }
#endif
#if defined(GPIOD)
  if (port == GPIOD) {
    crm_periph_clock_enable(CRM_GPIOD_PERIPH_CLOCK, TRUE);
    return;
  }
#endif
#if defined(GPIOE)
  if (port == GPIOE) {
    crm_periph_clock_enable(CRM_GPIOE_PERIPH_CLOCK, TRUE);
    return;
  }
#endif
#if defined(GPIOF)
  if (port == GPIOF) {
    crm_periph_clock_enable(CRM_GPIOF_PERIPH_CLOCK, TRUE);
    return;
  }
#endif
#elif defined(TRACKBALL_GPIO_BACKEND_STM32)
  if (port == NULL) {
    return;
  }
#if defined(GPIOA)
  if (port == GPIOA) {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    return;
  }
#endif
#if defined(GPIOB)
  if (port == GPIOB) {
    __HAL_RCC_GPIOB_CLK_ENABLE();
    return;
  }
#endif
#if defined(GPIOC)
  if (port == GPIOC) {
    __HAL_RCC_GPIOC_CLK_ENABLE();
    return;
  }
#endif
#if defined(GPIOD)
  if (port == GPIOD) {
    __HAL_RCC_GPIOD_CLK_ENABLE();
    return;
  }
#endif
#if defined(GPIOE)
  if (port == GPIOE) {
    __HAL_RCC_GPIOE_CLK_ENABLE();
    return;
  }
#endif
#if defined(GPIOF)
  if (port == GPIOF) {
    __HAL_RCC_GPIOF_CLK_ENABLE();
    return;
  }
#endif
#endif
}

static void trackball_init_motion_pin(void) {
  trackball_enable_gpio_clock(TRACKBALL_MOTION_PORT);

#if defined(TRACKBALL_GPIO_BACKEND_AT32)
  gpio_init_type gpio_init_struct;

  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_pins = TRACKBALL_MOTION_PIN;
  gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
  gpio_init_struct.gpio_pull =
#if TRACKBALL_MOTION_PULL_UP
      GPIO_PULL_UP;
#else
      GPIO_PULL_NONE;
#endif
  gpio_init(TRACKBALL_MOTION_PORT, &gpio_init_struct);
#elif defined(TRACKBALL_GPIO_BACKEND_STM32)
  GPIO_InitTypeDef gpio_init = {0};

  gpio_init.Pin = TRACKBALL_MOTION_PIN;
  gpio_init.Mode = GPIO_MODE_INPUT;
  gpio_init.Pull =
#if TRACKBALL_MOTION_PULL_UP
      GPIO_PULLUP;
#else
      GPIO_NOPULL;
#endif
  gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(TRACKBALL_MOTION_PORT, &gpio_init);
#endif
}

static bool trackball_motion_pending(void) {
#if defined(TRACKBALL_GPIO_BACKEND_AT32)
  const bool active =
      gpio_input_data_bit_read(TRACKBALL_MOTION_PORT, TRACKBALL_MOTION_PIN) != RESET;
#elif defined(TRACKBALL_GPIO_BACKEND_STM32)
  const bool active =
      HAL_GPIO_ReadPin(TRACKBALL_MOTION_PORT, TRACKBALL_MOTION_PIN) == GPIO_PIN_SET;
#endif

#if TRACKBALL_MOTION_ACTIVE_LOW
  return !active;
#else
  return active;
#endif
}
#else
static void trackball_init_motion_pin(void) {}
static bool trackball_motion_pending(void) { return true; }
#endif

static bool trackball_spi_begin(void) {
  return spi_bus_acquire(&trackball_state.bus_config);
}

static void trackball_spi_end(void) {
  spi_bus_release(&trackball_state.bus_config);
}

static void trackball_spi_reset_port(const trackball_spi_protocol_t *protocol) {
  spi_cs_deselect(&trackball_state.chip_select);
  trackball_delay_ns(protocol->ncs_sclk_ns);
  spi_cs_select(&trackball_state.chip_select);
  trackball_delay_ns(protocol->ncs_sclk_ns);
  spi_cs_deselect(&trackball_state.chip_select);
  trackball_delay_ns(protocol->ncs_sclk_ns);
}

static bool trackball_read_reg(uint8_t reg, uint8_t *value) {
  const trackball_spi_protocol_t *protocol = trackball_sensor_api.spi;
  const uint8_t address = reg & 0x7Fu;

  if (!trackball_spi_begin()) {
    return false;
  }

  spi_cs_select(&trackball_state.chip_select);
  trackball_delay_ns(protocol->ncs_sclk_ns);

  if (!spi_bus_write(&trackball_state.bus_config, &address, 1u)) {
    spi_cs_deselect(&trackball_state.chip_select);
    trackball_spi_end();
    return false;
  }

  if (protocol->read_addr_delay_us > 0u) {
    trackball_delay_us(protocol->read_addr_delay_us);
  }
  if (!spi_bus_read(&trackball_state.bus_config, value, 1u)) {
    spi_cs_deselect(&trackball_state.chip_select);
    trackball_spi_end();
    return false;
  }

  trackball_delay_ns(protocol->ncs_sclk_ns);
  spi_cs_deselect(&trackball_state.chip_select);
  trackball_spi_end();
  if (protocol->read_post_us > 0u) {
    trackball_delay_us(protocol->read_post_us);
  }
  return true;
}

static bool trackball_write_reg(uint8_t reg, uint8_t value) {
  const trackball_spi_protocol_t *protocol = trackball_sensor_api.spi;
  const uint8_t tx[2] = {(uint8_t)(reg | protocol->write_bit), value};

  if (!trackball_spi_begin()) {
    return false;
  }

  spi_cs_select(&trackball_state.chip_select);
  trackball_delay_ns(protocol->ncs_sclk_ns);
  if (!spi_bus_write(&trackball_state.bus_config, tx, M_ARRAY_SIZE(tx))) {
    spi_cs_deselect(&trackball_state.chip_select);
    trackball_spi_end();
    return false;
  }

  if (protocol->write_pre_deselect_us > 0u) {
    trackball_delay_us(protocol->write_pre_deselect_us);
  } else {
    trackball_delay_ns(protocol->ncs_sclk_ns);
  }
  spi_cs_deselect(&trackball_state.chip_select);
  trackball_spi_end();
  if (protocol->write_post_us > 0u) {
    trackball_delay_us(protocol->write_post_us);
  }
  return true;
}

static bool trackball_read_burst(uint8_t reg, uint8_t *data, size_t len) {
  const trackball_spi_protocol_t *protocol = trackball_sensor_api.spi;
  const uint8_t address = reg & 0x7Fu;

  if (!trackball_spi_begin()) {
    return false;
  }

  spi_cs_select(&trackball_state.chip_select);
  trackball_delay_ns(protocol->ncs_sclk_ns);
  if (!spi_bus_write(&trackball_state.bus_config, &address, 1u)) {
    spi_cs_deselect(&trackball_state.chip_select);
    trackball_spi_end();
    return false;
  }

  if (protocol->burst_addr_delay_us > 0u) {
    trackball_delay_us(protocol->burst_addr_delay_us);
  }
  if (!spi_bus_read(&trackball_state.bus_config, data, len)) {
    spi_cs_deselect(&trackball_state.chip_select);
    trackball_spi_end();
    return false;
  }

  trackball_delay_ns(protocol->ncs_sclk_ns);
  spi_cs_deselect(&trackball_state.chip_select);
  trackball_spi_end();
  if (protocol->burst_post_us > 0u) {
    trackball_delay_us(protocol->burst_post_us);
  } else if (protocol->burst_exit_ns > 0u) {
    trackball_delay_ns(protocol->burst_exit_ns);
  }
  return true;
}

static bool trackball_write_registers(const trackball_register_write_t *writes,
                                      size_t len) {
  if (writes == NULL) {
    return false;
  }

  for (size_t i = 0; i < len; i++) {
    if (!trackball_write_reg(writes[i].reg, writes[i].value)) {
      return false;
    }
  }

  return true;
}

#if defined(TRACKBALL_SENSOR_PMW3360)
static const trackball_spi_protocol_t pmw3360_spi_protocol = {
    .write_bit = PMW3360_SPI_WRITE_BIT,
    .ncs_sclk_ns = 120u,
    .read_addr_delay_us = 160u,
    .read_post_us = 20u,
    .write_pre_deselect_us = 35u,
    .write_post_us = 180u,
    .burst_addr_delay_us = 35u,
    .burst_post_us = 1u,
    .burst_exit_ns = 0u,
};

static bool pmw3360_set_cpi(uint16_t cpi) {
  uint16_t clamped = cpi;

  if (clamped < 100u) {
    clamped = 100u;
  } else if (clamped > 12000u) {
    clamped = 12000u;
  }

  return trackball_write_reg(PMW3360_REG_CONFIG1,
                             (uint8_t)((clamped / 100u) - 1u));
}

static bool pmw3360_init(void) {
  uint8_t product_id = 0;

  if (!trackball_write_reg(PMW3360_REG_POWER_UP_RESET,
                           PMW3360_POWER_UP_RESET_VALUE)) {
    return false;
  }
  timer_delay(50u);

  if (!trackball_read_reg(PMW3360_REG_PRODUCT_ID, &product_id) ||
      product_id != PMW3360_PRODUCT_ID) {
    return false;
  }

  (void)trackball_read_reg(PMW3360_REG_MOTION, &product_id);
  (void)trackball_read_reg(PMW3360_REG_DELTA_X_L, &product_id);
  (void)trackball_read_reg(PMW3360_REG_DELTA_X_H, &product_id);
  (void)trackball_read_reg(PMW3360_REG_DELTA_Y_L, &product_id);
  (void)trackball_read_reg(PMW3360_REG_DELTA_Y_H, &product_id);
  (void)trackball_write_reg(PMW3360_REG_CONFIG2, 0x00u);
  return true;
}

static bool pmw3360_start_burst(void) {
  return trackball_write_reg(PMW3360_REG_MOTION_BURST, 0x00u);
}

static bool pmw3360_read_motion(trackball_motion_sample_t *sample) {
  uint8_t burst_data[PMW3360_MOTION_BURST_LENGTH];

  if (!trackball_read_burst(PMW3360_REG_MOTION_BURST, burst_data,
                            M_ARRAY_SIZE(burst_data))) {
    return false;
  }

  sample->restart_burst = (burst_data[0] & 0x07u) != 0u;
  sample->motion = (burst_data[0] & PMW3360_MOTION_FLAG_MOTION) != 0u &&
                   (burst_data[0] & PMW3360_MOTION_FLAG_LIFTED) == 0u;
  sample->dx =
      (int16_t)((uint16_t)burst_data[2] | ((uint16_t)burst_data[3] << 8));
  sample->dy =
      (int16_t)((uint16_t)burst_data[4] | ((uint16_t)burst_data[5] << 8));
  return true;
}
#endif

#if defined(TRACKBALL_SENSOR_PAW3395)
static const trackball_spi_protocol_t paw3395_spi_protocol = {
    .write_bit = 0x80u,
    .ncs_sclk_ns = 120u,
    .read_addr_delay_us = 2u,
    .read_post_us = 2u,
    .write_pre_deselect_us = 0u,
    .write_post_us = 5u,
    .burst_addr_delay_us = 2u,
    .burst_post_us = 0u,
    .burst_exit_ns = 500u,
};

static const trackball_register_write_t paw3395_power_up_init_registers[] = {
    {0x7F, 0x07}, {0x40, 0x41}, {0x7F, 0x00}, {0x40, 0x80}, {0x7F, 0x0E},
    {0x55, 0x0D}, {0x56, 0x1B}, {0x57, 0xE8}, {0x58, 0xD5}, {0x7F, 0x14},
    {0x42, 0xBC}, {0x43, 0x74}, {0x4B, 0x20}, {0x4D, 0x00}, {0x53, 0x0E},
    {0x7F, 0x05}, {0x44, 0x04}, {0x4D, 0x06}, {0x51, 0x40}, {0x53, 0x40},
    {0x55, 0xCA}, {0x5A, 0xE8}, {0x5B, 0xEA}, {0x61, 0x31}, {0x62, 0x64},
    {0x6D, 0xB8}, {0x6E, 0x0F}, {0x70, 0x02}, {0x4A, 0x2A}, {0x60, 0x26},
    {0x7F, 0x06}, {0x6D, 0x70}, {0x6E, 0x60}, {0x6F, 0x04}, {0x53, 0x02},
    {0x55, 0x11}, {0x7A, 0x01}, {0x7D, 0x51}, {0x7F, 0x07}, {0x41, 0x10},
    {0x42, 0x32}, {0x43, 0x00}, {0x7F, 0x08}, {0x71, 0x4F}, {0x7F, 0x09},
    {0x62, 0x1F}, {0x63, 0x1F}, {0x65, 0x03}, {0x66, 0x03}, {0x67, 0x1F},
    {0x68, 0x1F}, {0x69, 0x03}, {0x6A, 0x03}, {0x6C, 0x1F}, {0x6D, 0x1F},
    {0x51, 0x04}, {0x53, 0x20}, {0x54, 0x20}, {0x71, 0x0C}, {0x72, 0x07},
    {0x73, 0x07}, {0x7F, 0x0A}, {0x4A, 0x14}, {0x4C, 0x14}, {0x55, 0x19},
    {0x7F, 0x14}, {0x4B, 0x30}, {0x4C, 0x03}, {0x61, 0x0B}, {0x62, 0x0A},
    {0x63, 0x02}, {0x7F, 0x15}, {0x4C, 0x02}, {0x56, 0x02}, {0x41, 0x91},
    {0x4D, 0x0A}, {0x7F, 0x0C}, {0x4A, 0x10}, {0x4B, 0x0C}, {0x4C, 0x40},
    {0x41, 0x25}, {0x55, 0x18}, {0x56, 0x14}, {0x49, 0x0A}, {0x42, 0x00},
    {0x43, 0x2D}, {0x44, 0x0C}, {0x54, 0x1A}, {0x5A, 0x0D}, {0x5F, 0x1E},
    {0x5B, 0x05}, {0x5E, 0x0F}, {0x7F, 0x0D}, {0x48, 0xDD}, {0x4F, 0x03},
    {0x52, 0x49}, {0x51, 0x00}, {0x54, 0x5B}, {0x53, 0x00}, {0x56, 0x64},
    {0x55, 0x00}, {0x58, 0xA5}, {0x57, 0x02}, {0x5A, 0x29}, {0x5B, 0x47},
    {0x5C, 0x81}, {0x5D, 0x40}, {0x71, 0xDC}, {0x70, 0x07}, {0x73, 0x00},
    {0x72, 0x08}, {0x75, 0xDC}, {0x74, 0x07}, {0x77, 0x00}, {0x76, 0x08},
    {0x7F, 0x10}, {0x4C, 0xD0}, {0x7F, 0x00}, {0x4F, 0x63}, {0x4E, 0x00},
    {0x52, 0x63}, {0x51, 0x00}, {0x54, 0x54}, {0x5A, 0x10}, {0x77, 0x4F},
    {0x47, 0x01}, {0x5B, 0x40}, {0x64, 0x60}, {0x65, 0x06}, {0x66, 0x13},
    {0x67, 0x0F}, {0x78, 0x01}, {0x79, 0x9C}, {0x40, 0x00}, {0x55, 0x02},
    {0x23, 0x70}, {0x22, 0x01},
};

static bool paw3395_verify_identity(void) {
  uint8_t product_id = 0;
  uint8_t inverse_product_id = 0;

  return trackball_read_reg(PAW3395_REG_PRODUCT_ID, &product_id) &&
         trackball_read_reg(PAW3395_REG_INV_PRODUCT_ID, &inverse_product_id) &&
         product_id == PAW3395_PRODUCT_ID &&
         inverse_product_id == PAW3395_INV_PRODUCT_ID;
}

static bool paw3395_finish_boot_sequence(void) {
  uint8_t boot_status = 0;

  timer_delay(1u);
  for (uint8_t attempt = 0; attempt < 2u; attempt++) {
    for (uint8_t i = 0; i < 60u; i++) {
      if (!trackball_read_reg(PAW3395_REG_BOOT_STATUS, &boot_status)) {
        return false;
      }
      if (boot_status == PAW3395_BOOT_STATUS_READY) {
        return trackball_write_reg(0x22u, 0x00u) &&
               trackball_write_reg(0x55u, 0x00u) &&
               trackball_write_reg(PAW3395_REG_PAGE_SELECT, 0x07u) &&
               trackball_write_reg(0x40u, 0x40u) &&
               trackball_write_reg(PAW3395_REG_PAGE_SELECT, 0x00u);
      }
      timer_delay(1u);
    }

    if (attempt == 0u &&
        (!trackball_write_reg(PAW3395_REG_PAGE_SELECT, 0x14u) ||
         !trackball_write_reg(PAW3395_REG_BOOT_STATUS, 0x00u) ||
         !trackball_write_reg(PAW3395_REG_PAGE_SELECT, 0x00u))) {
      return false;
    }
  }

  return false;
}

static bool paw3395_load_power_up_initialization(void) {
  return trackball_write_registers(paw3395_power_up_init_registers,
                                   M_ARRAY_SIZE(paw3395_power_up_init_registers)) &&
         paw3395_finish_boot_sequence();
}

static bool paw3395_init(void) {
  uint8_t scratch = 0;

  timer_delay(50u);
  trackball_spi_reset_port(&paw3395_spi_protocol);

  if (!trackball_write_reg(PAW3395_REG_POWER_UP_RESET,
                           PAW3395_POWER_UP_RESET_VALUE)) {
    return false;
  }
  timer_delay(5u);

  if (!paw3395_load_power_up_initialization() || !paw3395_verify_identity()) {
    return false;
  }

  for (uint8_t reg = PAW3395_REG_MOTION; reg <= PAW3395_REG_DELTA_Y_H; reg++) {
    if (!trackball_read_reg(reg, &scratch)) {
      return false;
    }
  }

  return true;
}

static uint8_t paw3395_motion_ctrl_value(void) {
  uint8_t value = 0x02u;

#if !TRACKBALL_MOTION_ACTIVE_LOW
  value |= 0x80u;
#endif

  return value;
}

static bool paw3395_set_cpi(uint16_t cpi) {
  uint32_t clamped = cpi;

  if (clamped < PAW3395_CPI_MIN) {
    clamped = PAW3395_CPI_MIN;
  } else if (clamped > PAW3395_CPI_MAX) {
    clamped = PAW3395_CPI_MAX;
  }

  uint16_t encoded = (uint16_t)(((clamped + (PAW3395_CPI_STEP / 2u)) /
                                 PAW3395_CPI_STEP) -
                                1u);

  return trackball_write_reg(PAW3395_REG_MOTION_CTRL,
                             paw3395_motion_ctrl_value()) &&
         trackball_write_reg(PAW3395_REG_RESOLUTION_X_LOW,
                             (uint8_t)(encoded & 0xFFu)) &&
         trackball_write_reg(PAW3395_REG_RESOLUTION_X_HIGH,
                             (uint8_t)(encoded >> 8)) &&
         trackball_write_reg(PAW3395_REG_RESOLUTION_Y_LOW,
                             (uint8_t)(encoded & 0xFFu)) &&
         trackball_write_reg(PAW3395_REG_RESOLUTION_Y_HIGH,
                             (uint8_t)(encoded >> 8)) &&
         trackball_write_reg(PAW3395_REG_SET_RESOLUTION, 0x01u);
}

static bool paw3395_start_burst(void) { return true; }

static bool paw3395_read_motion(trackball_motion_sample_t *sample) {
  uint8_t burst_data[PAW3395_MOTION_BURST_LENGTH];

  if (!trackball_read_burst(PAW3395_REG_MOTION_BURST, burst_data,
                            M_ARRAY_SIZE(burst_data))) {
    return false;
  }

  sample->restart_burst = true;
  sample->motion = (burst_data[0] & PAW3395_MOTION_FLAG_MOTION) != 0u &&
                   (burst_data[0] & PAW3395_MOTION_FLAG_LIFTED) == 0u;
  sample->dx =
      (int16_t)((uint16_t)burst_data[2] | ((uint16_t)burst_data[3] << 8));
  sample->dy =
      (int16_t)((uint16_t)burst_data[4] | ((uint16_t)burst_data[5] << 8));
  return true;
}
#endif

#if defined(TRACKBALL_SENSOR_PMW3360)
static const trackball_sensor_api_t trackball_sensor_api = {
    .name = TRACKBALL_SENSOR_NAME,
    .spi_mode = TRACKBALL_SPI_MODE,
    .spi = &pmw3360_spi_protocol,
    .init = pmw3360_init,
    .set_cpi = pmw3360_set_cpi,
    .start_burst = pmw3360_start_burst,
    .read_motion = pmw3360_read_motion,
};
#elif defined(TRACKBALL_SENSOR_PAW3395)
static const trackball_sensor_api_t trackball_sensor_api = {
    .name = TRACKBALL_SENSOR_NAME,
    .spi_mode = TRACKBALL_SPI_MODE,
    .spi = &paw3395_spi_protocol,
    .init = paw3395_init,
    .set_cpi = paw3395_set_cpi,
    .start_burst = paw3395_start_burst,
    .read_motion = paw3395_read_motion,
};
#endif

static bool trackball_restore_cpi(void) {
  if (trackball_sensor_api.set_cpi == NULL) {
    return true;
  }

  return trackball_sensor_api.set_cpi(trackball_state.current_cpi);
}

static void trackball_mark_unavailable(uint32_t now) {
  trackball_state.enabled = false;
  trackball_state.burst_mode_started = false;
  trackball_state.consecutive_errors = 0;
  trackball_state.last_recovery_attempt_ms = now;
}

static bool trackball_try_enable(uint32_t now) {
  if (!trackball_sensor_api.init() || !trackball_restore_cpi()) {
    trackball_mark_unavailable(now);
    return false;
  }

  trackball_state.enabled = true;
  trackball_state.burst_mode_started = false;
  trackball_state.consecutive_errors = 0;
  trackball_state.last_poll_ms = now;
  return true;
}

static void trackball_handle_comm_error(uint32_t now) {
  trackball_state.burst_mode_started = false;

  if (trackball_state.consecutive_errors < UINT8_MAX) {
    trackball_state.consecutive_errors++;
  }

  if (trackball_state.consecutive_errors >= TRACKBALL_MAX_CONSECUTIVE_ERRORS) {
    trackball_mark_unavailable(now);
  }
}

static void trackball_try_recover(uint32_t now) {
  if (trackball_state.enabled ||
      timer_elapsed(trackball_state.last_recovery_attempt_ms) <
          TRACKBALL_RECOVERY_RETRY_MS) {
    return;
  }

  trackball_state.last_recovery_attempt_ms = now;
  (void)trackball_try_enable(now);
}

static int8_t trackball_clamp_i16_to_i8(int16_t value) {
  if (value > INT8_MAX) {
    return INT8_MAX;
  }
  if (value < INT8_MIN) {
    return INT8_MIN;
  }
  return (int8_t)value;
}

static int8_t trackball_consume_mouse_accum(int32_t *accum) {
  int32_t whole = *accum / TRACKBALL_MOUSE_FP_ONE;

  if (whole > INT8_MAX) {
    whole = INT8_MAX;
  } else if (whole < INT8_MIN) {
    whole = INT8_MIN;
  }

  *accum -= whole * TRACKBALL_MOUSE_FP_ONE;
  return trackball_clamp_i16_to_i8((int16_t)whole);
}

static void trackball_apply_sniper_scaling(int32_t *dx_fp, int32_t *dy_fp) {
  if (is_sniper_active) {
    *dx_fp = (*dx_fp * (int32_t)eeconfig->options.sniper_mode_multiplier) / 255;
    *dy_fp = (*dy_fp * (int32_t)eeconfig->options.sniper_mode_multiplier) / 255;
  }
}

static int32_t trackball_vector_delta_fp(uint32_t magnitude, uint8_t speed,
                                         uint8_t acceleration) {
  const int64_t max_sq =
      (int64_t)TRACKBALL_VECTOR_MAX * (int64_t)TRACKBALL_VECTOR_MAX;
  const int64_t mag_sq = (int64_t)magnitude * (int64_t)magnitude;
  const int64_t curve_term = ((int64_t)(255u - acceleration) * max_sq +
                              (int64_t)acceleration * mag_sq) /
                             255LL;
  int64_t numerator = (int64_t)magnitude * curve_term;
  numerator *= (int64_t)speed;
  numerator *= (int64_t)TRACKBALL_MOUSE_FP_ONE;

  int64_t denominator = 16384LL * (int64_t)TRACKBALL_MOUSE_DIVISOR;
  int64_t delta_fp = numerator / denominator;

  if (delta_fp > INT32_MAX) {
    return INT32_MAX;
  }
  return (int32_t)delta_fp;
}

static void trackball_apply_exponential_smoothing(int32_t raw_dx, int32_t raw_dy,
                                                  int32_t *out_dx,
                                                  int32_t *out_dy) {
  const uint8_t smoothing = trackball_config_cache.smoothing;

  if (smoothing == 0u) {
    *out_dx = raw_dx;
    *out_dy = raw_dy;
    return;
  }

  trackball_filtered_dx +=
      (raw_dx - trackball_filtered_dx) / (1L << smoothing);
  trackball_filtered_dy +=
      (raw_dy - trackball_filtered_dy) / (1L << smoothing);
  *out_dx = trackball_filtered_dx;
  *out_dy = trackball_filtered_dy;
}

static void trackball_emit_motion(int16_t dx, int16_t dy) {
#if TRACKBALL_SWAP_XY
  {
    const int16_t swapped_dx = dy;
    dy = dx;
    dx = swapped_dx;
  }
#endif
  if (trackball_config_cache.swap_xy) {
    const int16_t swapped_dx = dy;
    dy = dx;
    dx = swapped_dx;
  }
#if TRACKBALL_INVERT_X
  dx = (int16_t)-dx;
#endif
  if (trackball_config_cache.invert_x) {
    dx = (int16_t)-dx;
  }
#if TRACKBALL_INVERT_Y
  dy = (int16_t)-dy;
#endif
  if (trackball_config_cache.invert_y) {
    dy = (int16_t)-dy;
  }

  int32_t smoothed_dx = 0;
  int32_t smoothed_dy = 0;
  trackball_apply_exponential_smoothing((int32_t)dx, (int32_t)dy,
                                        &smoothed_dx, &smoothed_dy);

  const int64_t mag_sq =
      (int64_t)smoothed_dx * (int64_t)smoothed_dx +
      (int64_t)smoothed_dy * (int64_t)smoothed_dy;
  const uint32_t magnitude =
      mag_sq > UINT32_MAX ? UINT32_MAX : (uint32_t)mag_sq;

  if (magnitude == 0u) {
    return;
  }

  const uint32_t magnitude_len = usqrt32(magnitude);
  const uint8_t speed = trackball_config_cache.mouse_speed;
  const uint8_t acceleration = trackball_config_cache.mouse_acceleration;
  const int32_t delta_fp =
      trackball_vector_delta_fp(magnitude_len, speed, acceleration);

  int32_t dx_fp = (int64_t)smoothed_dx * delta_fp / (int32_t)magnitude_len;
  int32_t dy_fp = (int64_t)smoothed_dy * delta_fp / (int32_t)magnitude_len;

  trackball_apply_sniper_scaling(&dx_fp, &dy_fp);

  trackball_mouse_accum_x += dx_fp;
  trackball_mouse_accum_y += dy_fp;

  const int8_t out_x = trackball_consume_mouse_accum(&trackball_mouse_accum_x);
  const int8_t out_y = trackball_consume_mouse_accum(&trackball_mouse_accum_y);

  if (out_x != 0 || out_y != 0) {
    hid_mouse_move(out_x, out_y, 0u);
  }
}

void trackball_init(void) {
  const uint32_t now = timer_read();

  trackball_state.bus_config.mode = trackball_sensor_api.spi_mode;
  spi_cs_init(&trackball_state.chip_select);
  trackball_init_motion_pin();

  trackball_config_t config;
  if (eeconfig != NULL) {
    memcpy(&config, &CURRENT_PROFILE.trackball_config, sizeof(config));
  } else {
    trackball_init_default_config(&config);
  }
  trackball_apply_config(config);

  trackball_mouse_accum_x = 0;
  trackball_mouse_accum_y = 0;
  trackball_filtered_dx = 0;
  trackball_filtered_dy = 0;
  trackball_last_mouse_tick = now;

  trackball_state.last_recovery_attempt_ms = now;
  (void)trackball_try_enable(now);
}

void trackball_task(void) {
  trackball_motion_sample_t sample = {0};
  const uint32_t now = timer_read();

  if (!trackball_state.enabled) {
    trackball_try_recover(now);
    return;
  }

  if (timer_elapsed(trackball_state.last_poll_ms) < TRACKBALL_POLL_INTERVAL_MS) {
    return;
  }
  trackball_state.last_poll_ms = now;

  if (!trackball_motion_pending()) {
    return;
  }

  if (!trackball_state.burst_mode_started) {
    if (!trackball_sensor_api.start_burst()) {
      trackball_handle_comm_error(now);
      return;
    }
    trackball_state.burst_mode_started = true;
  }

  if (!trackball_sensor_api.read_motion(&sample)) {
    trackball_handle_comm_error(now);
    return;
  }

  trackball_state.consecutive_errors = 0;

  if (sample.restart_burst) {
    trackball_state.burst_mode_started = false;
  }

  if (!sample.motion) {
    return;
  }

  trackball_state.last_dx = sample.dx;
  trackball_state.last_dy = sample.dy;

  if (!trackball_config_cache.enabled) {
    return;
  }

  trackball_emit_motion(sample.dx, sample.dy);
}

void trackball_select_mouse_preset(trackball_config_t *config, uint8_t preset) {
  if (config == NULL) {
    return;
  }

  *config = trackball_normalize_config(*config);
  config->active_mouse_preset = preset % TRACKBALL_MOUSE_PRESET_COUNT;
  config->mouse_speed =
      config->mouse_presets[config->active_mouse_preset].mouse_speed;
  config->mouse_acceleration =
      config->mouse_presets[config->active_mouse_preset].mouse_acceleration;
}

void trackball_get_state(trackball_diagnostic_state_t *state) {
  if (state == NULL) {
    return;
  }
  state->enabled = trackball_state.enabled;
  state->current_cpi = trackball_state.current_cpi;
  state->last_dx = trackball_state.last_dx;
  state->last_dy = trackball_state.last_dy;
}

static void trackball_set_cpi(uint16_t cpi) {
  const uint32_t now = timer_read();

  trackball_state.current_cpi = cpi;
  trackball_config_cache.cpi = cpi;

  if (!trackball_state.enabled) {
    return;
  }
  if (trackball_sensor_api.set_cpi != NULL) {
    if (trackball_sensor_api.set_cpi(cpi)) {
#if defined(RGB_ENABLED)
      rgb_flash_cpi(cpi);
#endif
    } else {
      trackball_handle_comm_error(now);
    }
  }
}

trackball_config_t trackball_get_config(void) {
  return trackball_config_cache;
}

void trackball_apply_config(trackball_config_t config) {
  const trackball_config_t normalized = trackball_normalize_config(config);
  trackball_config_cache = normalized;
  trackball_state.current_cpi = normalized.cpi;

  if (trackball_state.enabled && trackball_sensor_api.set_cpi != NULL) {
    (void)trackball_sensor_api.set_cpi(normalized.cpi);
  }

  trackball_mouse_accum_x = 0;
  trackball_mouse_accum_y = 0;
  trackball_filtered_dx = 0;
  trackball_filtered_dy = 0;
}

void trackball_set_config(trackball_config_t config) {
  const trackball_config_t normalized = trackball_normalize_config(config);
  if (eeconfig != NULL) {
    const uint32_t addr =
        offsetof(eeconfig_t, profiles) +
        eeconfig->current_profile * sizeof(eeconfig_profile_t) +
        offsetof(eeconfig_profile_t, trackball_config);
    (void)wear_leveling_write(addr, &normalized, sizeof(normalized));
  }
  trackball_apply_config(normalized);
}

void trackball_select_next_preset(void) {
  trackball_config_t config = trackball_get_config();
  trackball_select_mouse_preset(
      &config, (uint8_t)((config.active_mouse_preset + 1u) %
                         TRACKBALL_MOUSE_PRESET_COUNT));
  trackball_set_config(config);
}

void trackball_increase_cpi(void) {
#if defined(TRACKBALL_SENSOR_PAW3395)
  trackball_set_cpi(trackball_state.current_cpi + 50);
#else
  trackball_set_cpi(trackball_state.current_cpi + 100);
#endif
}

void trackball_decrease_cpi(void) {
#if defined(TRACKBALL_SENSOR_PAW3395)
  if (trackball_state.current_cpi > 50) {
    trackball_set_cpi(trackball_state.current_cpi - 50);
  }
#else
  if (trackball_state.current_cpi > 100) {
    trackball_set_cpi(trackball_state.current_cpi - 100);
  }
#endif
}

#else

void trackball_init(void) {}
void trackball_task(void) {}
void trackball_get_state(trackball_diagnostic_state_t *state) {}
trackball_config_t trackball_get_config(void) {
  trackball_config_t config;
  trackball_init_default_config(&config);
  return config;
}
void trackball_apply_config(trackball_config_t config) { (void)config; }
void trackball_set_config(trackball_config_t config) { (void)config; }
void trackball_increase_cpi(void) {}
void trackball_decrease_cpi(void) {}
void trackball_select_next_preset(void) {}

#endif
