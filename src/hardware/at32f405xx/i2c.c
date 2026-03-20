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

#include "hardware/hardware.h"

#include "at32f402_405.h"

#if I2C_NUM_BUSES > 4
#error "I2C_NUM_BUSES > 4 is not supported"
#endif

#if I2C_NUM_BUSES > 0
#if !defined(I2C_BUS0_INSTANCE) || !defined(I2C_BUS0_CLOCK) ||                 \
    !defined(I2C_BUS0_SCL_PORT) || !defined(I2C_BUS0_SCL_PIN) ||               \
    !defined(I2C_BUS0_SCL_PIN_SOURCE) || !defined(I2C_BUS0_SDA_PORT) ||        \
    !defined(I2C_BUS0_SDA_PIN) || !defined(I2C_BUS0_SDA_PIN_SOURCE) ||         \
    !defined(I2C_BUS0_PIN_MUX)
#error "I2C bus 0 configuration macros are incomplete"
#endif
#endif

#if I2C_NUM_BUSES > 1
#if !defined(I2C_BUS1_INSTANCE) || !defined(I2C_BUS1_CLOCK) ||                 \
    !defined(I2C_BUS1_SCL_PORT) || !defined(I2C_BUS1_SCL_PIN) ||               \
    !defined(I2C_BUS1_SCL_PIN_SOURCE) || !defined(I2C_BUS1_SDA_PORT) ||        \
    !defined(I2C_BUS1_SDA_PIN) || !defined(I2C_BUS1_SDA_PIN_SOURCE) ||         \
    !defined(I2C_BUS1_PIN_MUX)
#error "I2C bus 1 configuration macros are incomplete"
#endif
#endif

#if I2C_NUM_BUSES > 2
#if !defined(I2C_BUS2_INSTANCE) || !defined(I2C_BUS2_CLOCK) ||                 \
    !defined(I2C_BUS2_SCL_PORT) || !defined(I2C_BUS2_SCL_PIN) ||               \
    !defined(I2C_BUS2_SCL_PIN_SOURCE) || !defined(I2C_BUS2_SDA_PORT) ||        \
    !defined(I2C_BUS2_SDA_PIN) || !defined(I2C_BUS2_SDA_PIN_SOURCE) ||         \
    !defined(I2C_BUS2_PIN_MUX)
#error "I2C bus 2 configuration macros are incomplete"
#endif
#endif

#if I2C_NUM_BUSES > 3
#if !defined(I2C_BUS3_INSTANCE) || !defined(I2C_BUS3_CLOCK) ||                 \
    !defined(I2C_BUS3_SCL_PORT) || !defined(I2C_BUS3_SCL_PIN) ||               \
    !defined(I2C_BUS3_SCL_PIN_SOURCE) || !defined(I2C_BUS3_SDA_PORT) ||        \
    !defined(I2C_BUS3_SDA_PIN) || !defined(I2C_BUS3_SDA_PIN_SOURCE) ||         \
    !defined(I2C_BUS3_PIN_MUX)
#error "I2C bus 3 configuration macros are incomplete"
#endif
#endif

#if I2C_NUM_BUSES > 0
static uint32_t i2c_timeout_cycles(void) {
  return (uint32_t)(((uint64_t)F_CPU * I2C_TRANSFER_TIMEOUT_MS) / 1000ULL);
}

static void i2c_enable_gpio_clock(gpio_type *port) {
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
}

typedef struct {
  i2c_type *instance;
  gpio_type *scl_port;
  uint16_t scl_pin;
  uint16_t scl_pin_source;
  gpio_type *sda_port;
  uint16_t sda_pin;
  uint16_t sda_pin_source;
  uint16_t pin_mux;
  bool configured;
  uint32_t last_frequency_hz;
} i2c_bus_state_t;

#define I2C_AT32_BUS_ENTRY(index)                                              \
  {                                                                            \
      .instance = I2C_BUS##index##_INSTANCE,                                   \
      .scl_port = I2C_BUS##index##_SCL_PORT,                                   \
      .scl_pin = I2C_BUS##index##_SCL_PIN,                                     \
      .scl_pin_source = I2C_BUS##index##_SCL_PIN_SOURCE,                       \
      .sda_port = I2C_BUS##index##_SDA_PORT,                                   \
      .sda_pin = I2C_BUS##index##_SDA_PIN,                                     \
      .sda_pin_source = I2C_BUS##index##_SDA_PIN_SOURCE,                       \
      .pin_mux = I2C_BUS##index##_PIN_MUX,                                     \
      .configured = false,                                                     \
      .last_frequency_hz = 0,                                                  \
  }

static i2c_bus_state_t i2c_buses[] = {
#if I2C_NUM_BUSES > 0
    I2C_AT32_BUS_ENTRY(0),
#endif
#if I2C_NUM_BUSES > 1
    I2C_AT32_BUS_ENTRY(1),
#endif
#if I2C_NUM_BUSES > 2
    I2C_AT32_BUS_ENTRY(2),
#endif
#if I2C_NUM_BUSES > 3
    I2C_AT32_BUS_ENTRY(3),
#endif
};

static bool i2c_driver_initialized = false;

static void i2c_enable_bus_clock(uint8_t bus) {
  switch (bus) {
#if I2C_NUM_BUSES > 0
    case 0:
      crm_periph_clock_enable(I2C_BUS0_CLOCK, TRUE);
      return;
#endif
#if I2C_NUM_BUSES > 1
    case 1:
      crm_periph_clock_enable(I2C_BUS1_CLOCK, TRUE);
      return;
#endif
#if I2C_NUM_BUSES > 2
    case 2:
      crm_periph_clock_enable(I2C_BUS2_CLOCK, TRUE);
      return;
#endif
#if I2C_NUM_BUSES > 3
    case 3:
      crm_periph_clock_enable(I2C_BUS3_CLOCK, TRUE);
      return;
#endif
    default:
      return;
  }
}

static void i2c_configure_mux_pin(gpio_type *port, uint16_t pin,
                                  uint16_t pin_source, uint16_t pin_mux) {
  gpio_init_type gpio_init_struct;

  i2c_enable_gpio_clock(port);
  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_pins = pin;
  gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_OPEN_DRAIN;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init(port, &gpio_init_struct);
  gpio_pin_mux_config(port, pin_source, pin_mux);
}

static void i2c_reset_ctrl2(i2c_type *instance) {
  instance->ctrl2_bit.saddr = 0;
  instance->ctrl2_bit.readh10 = 0;
  instance->ctrl2_bit.cnt = 0;
  instance->ctrl2_bit.rlden = 0;
  instance->ctrl2_bit.dir = 0;
}

static void i2c_clear_flag_if_set(i2c_type *instance, uint32_t flag) {
  if (i2c_flag_get(instance, flag) != RESET) {
    i2c_flag_clear(instance, flag);
  }
}

static void i2c_abort_transfer(i2c_type *instance) {
  i2c_stop_generate(instance);
  i2c_clear_flag_if_set(instance, I2C_STOPF_FLAG);
  i2c_clear_flag_if_set(instance, I2C_ACKFAIL_FLAG);
  i2c_clear_flag_if_set(instance, I2C_BUSERR_FLAG);
  i2c_clear_flag_if_set(instance, I2C_ARLOST_FLAG);
  i2c_clear_flag_if_set(instance, I2C_OUF_FLAG);
  i2c_reset_ctrl2(instance);
}

static bool i2c_wait_for_flag(i2c_type *instance, uint32_t flag, bool set) {
  uint32_t start = board_cycle_count();
  uint32_t timeout = i2c_timeout_cycles();

  while ((i2c_flag_get(instance, flag) != RESET) != set) {
    if (i2c_flag_get(instance, I2C_ACKFAIL_FLAG) != RESET ||
        i2c_flag_get(instance, I2C_BUSERR_FLAG) != RESET ||
        i2c_flag_get(instance, I2C_ARLOST_FLAG) != RESET ||
        i2c_flag_get(instance, I2C_OUF_FLAG) != RESET) {
      i2c_abort_transfer(instance);
      return false;
    }

    if ((uint32_t)(board_cycle_count() - start) > timeout) {
      i2c_abort_transfer(instance);
      return false;
    }
  }

  return true;
}

static uint32_t i2c_build_timing(uint32_t pclk_hz, uint32_t target_hz) {
  uint32_t period_ns;
  uint32_t low_min_ns;
  uint32_t high_min_ns;
  uint32_t rise_ns;
  uint32_t setup_ns;
  uint32_t low_ns;
  uint32_t high_ns;
  uint32_t prescaled_ns;
  uint32_t max_ticks_ns;
  uint32_t div;
  uint32_t scll;
  uint32_t sclh;
  uint32_t sdadel = 0;
  uint32_t scldel;

  if (target_hz == 0u || target_hz > 400000u || pclk_hz == 0u) {
    return 0u;
  }

  period_ns = 1000000000u / target_hz;
  if (target_hz <= 100000u) {
    low_min_ns = 4700u;
    high_min_ns = 4000u;
    rise_ns = 1000u;
    setup_ns = 250u;
  } else {
    low_min_ns = 1300u;
    high_min_ns = 600u;
    rise_ns = 300u;
    setup_ns = 100u;
  }

  low_ns = M_MAX(period_ns / 2u, low_min_ns);
  high_ns = M_MAX(period_ns - low_ns, high_min_ns);
  max_ticks_ns = M_MAX(low_ns, high_ns);
  div = (uint32_t)(((uint64_t)max_ticks_ns * pclk_hz + 254999999999ULL) /
                   255000000000ULL);
  if (div > 0u) {
    div--;
  }
  if (div > 255u) {
    div = 255u;
  }

  prescaled_ns =
      (uint32_t)(((uint64_t)1000000000ULL * (div + 1u)) / pclk_hz);
  if (prescaled_ns == 0u) {
    prescaled_ns = 1u;
  }

  scll = (low_ns + prescaled_ns - 1u) / prescaled_ns;
  if (scll > 0u) {
    scll--;
  }
  sclh = (high_ns + prescaled_ns - 1u) / prescaled_ns;
  if (sclh > 0u) {
    sclh--;
  }
  scldel = (setup_ns + rise_ns + prescaled_ns - 1u) / prescaled_ns;

  if (scll > 0xFFu) {
    scll = 0xFFu;
  }
  if (sclh > 0xFFu) {
    sclh = 0xFFu;
  }
  if (scldel == 0u) {
    scldel = 1u;
  }
  if (scldel > 0x0Fu) {
    scldel = 0x0Fu;
  }

  return scll | (sclh << 8) | (sdadel << 16) | (scldel << 20) |
         ((div & 0xFFu) << 24);
}

static bool i2c_configure_bus(i2c_bus_state_t *bus_state,
                              const i2c_bus_config_t *config) {
  crm_clocks_freq_type clocks;
  uint32_t timing;

  if (config->frequency_hz == 0u || config->frequency_hz > 400000u) {
    return false;
  }

  if (bus_state->configured &&
      bus_state->last_frequency_hz == config->frequency_hz) {
    return true;
  }

  crm_clocks_freq_get(&clocks);
  timing = i2c_build_timing(clocks.apb1_freq, config->frequency_hz);
  if (timing == 0u) {
    return false;
  }

  i2c_reset(bus_state->instance);
  i2c_init(bus_state->instance, 0u, timing);
  i2c_clock_stretch_enable(bus_state->instance, TRUE);
  i2c_ack_enable(bus_state->instance, TRUE);
  i2c_enable(bus_state->instance, TRUE);

  bus_state->configured = true;
  bus_state->last_frequency_hz = config->frequency_hz;
  return true;
}

static void i2c_start_transfer(i2c_type *instance, uint8_t address,
                               size_t remaining, i2c_start_mode_type start) {
  uint8_t chunk = remaining > 255u ? 255u : (uint8_t)remaining;
  i2c_transmit_set(instance, address, chunk,
                   remaining > 255u ? I2C_RELOAD_MODE : I2C_AUTO_STOP_MODE,
                   start);
}

static bool i2c_write_bytes(i2c_type *instance, uint8_t address,
                            const uint8_t *first, size_t first_len,
                            const uint8_t *second, size_t second_len) {
  size_t remaining = first_len + second_len;
  size_t sent = 0;

  if (!i2c_wait_for_flag(instance, I2C_BUSYF_FLAG, false)) {
    return false;
  }
  if (remaining == 0u) {
    return true;
  }

  i2c_start_transfer(instance, address, remaining, I2C_GEN_START_WRITE);
  while (remaining > 0u) {
    uint8_t value;

    if (!i2c_wait_for_flag(instance, I2C_TDIS_FLAG, true)) {
      return false;
    }

    if (sent < first_len) {
      value = first[sent];
    } else {
      value = second[sent - first_len];
    }
    i2c_data_send(instance, value);
    sent++;
    remaining--;

    if ((sent % 255u) == 0u && remaining > 0u) {
      if (!i2c_wait_for_flag(instance, I2C_TCRLD_FLAG, true)) {
        return false;
      }
      i2c_start_transfer(instance, address, remaining, I2C_WITHOUT_START);
    }
  }

  if (!i2c_wait_for_flag(instance, I2C_STOPF_FLAG, true)) {
    return false;
  }
  i2c_flag_clear(instance, I2C_STOPF_FLAG);
  i2c_reset_ctrl2(instance);
  return true;
}

static bool i2c_read_bytes(i2c_type *instance, uint8_t address, uint8_t *rx,
                           size_t len) {
  size_t remaining = len;
  size_t read = 0;

  if (!i2c_wait_for_flag(instance, I2C_BUSYF_FLAG, false)) {
    return false;
  }
  if (remaining == 0u) {
    return true;
  }

  i2c_start_transfer(instance, address, remaining, I2C_GEN_START_READ);
  while (remaining > 0u) {
    if (!i2c_wait_for_flag(instance, I2C_RDBF_FLAG, true)) {
      return false;
    }

    rx[read++] = i2c_data_receive(instance);
    remaining--;

    if ((read % 255u) == 0u && remaining > 0u) {
      if (!i2c_wait_for_flag(instance, I2C_TCRLD_FLAG, true)) {
        return false;
      }
      i2c_start_transfer(instance, address, remaining, I2C_WITHOUT_START);
    }
  }

  if (!i2c_wait_for_flag(instance, I2C_STOPF_FLAG, true)) {
    return false;
  }
  i2c_flag_clear(instance, I2C_STOPF_FLAG);
  i2c_reset_ctrl2(instance);
  return true;
}

static bool i2c_send_register_address(i2c_type *instance, uint8_t address,
                                      uint16_t reg,
                                      i2c_register_width_t register_width) {
  uint8_t reg_bytes[2];
  size_t reg_len = (size_t)register_width;

  if (register_width == I2C_REGISTER_8BIT) {
    reg_bytes[0] = (uint8_t)reg;
  } else if (register_width == I2C_REGISTER_16BIT) {
    reg_bytes[0] = (uint8_t)(reg >> 8);
    reg_bytes[1] = (uint8_t)reg;
  } else {
    return false;
  }

  if (!i2c_wait_for_flag(instance, I2C_BUSYF_FLAG, false)) {
    return false;
  }

  i2c_transmit_set(instance, address, (uint8_t)reg_len, I2C_SOFT_STOP_MODE,
                   I2C_GEN_START_WRITE);
  for (size_t i = 0; i < reg_len; i++) {
    if (!i2c_wait_for_flag(instance, I2C_TDIS_FLAG, true)) {
      return false;
    }
    i2c_data_send(instance, reg_bytes[i]);
  }

  return i2c_wait_for_flag(instance, I2C_TDC_FLAG, true);
}
#endif

void i2c_bus_init(void) {
#if I2C_NUM_BUSES > 0
  if (i2c_driver_initialized) {
    return;
  }

  for (uint8_t bus = 0; bus < M_ARRAY_SIZE(i2c_buses); bus++) {
    i2c_bus_state_t *bus_state = &i2c_buses[bus];
    i2c_enable_bus_clock(bus);
    i2c_configure_mux_pin(bus_state->scl_port, bus_state->scl_pin,
                          bus_state->scl_pin_source, bus_state->pin_mux);
    i2c_configure_mux_pin(bus_state->sda_port, bus_state->sda_pin,
                          bus_state->sda_pin_source, bus_state->pin_mux);
  }

  i2c_driver_initialized = true;
#endif
}

bool i2c_bus_acquire(const i2c_bus_config_t *config) {
#if I2C_NUM_BUSES > 0
  if (config == NULL || config->bus >= M_ARRAY_SIZE(i2c_buses)) {
    return false;
  }

  if (!i2c_driver_initialized) {
    i2c_bus_init();
  }

  return i2c_configure_bus(&i2c_buses[config->bus], config);
#else
  (void)config;
  return false;
#endif
}

void i2c_bus_release(const i2c_bus_config_t *config) { (void)config; }

bool i2c_bus_write(const i2c_bus_config_t *config, uint8_t address,
                   const uint8_t *tx, size_t len) {
#if I2C_NUM_BUSES > 0
  if (address > 0x7Fu || (len > 0u && tx == NULL) || !i2c_bus_acquire(config)) {
    return false;
  }

  return i2c_write_bytes(i2c_buses[config->bus].instance, address, tx, len, NULL,
                         0u);
#else
  (void)config;
  (void)address;
  (void)tx;
  (void)len;
  return false;
#endif
}

bool i2c_bus_read(const i2c_bus_config_t *config, uint8_t address, uint8_t *rx,
                  size_t len) {
#if I2C_NUM_BUSES > 0
  if (address > 0x7Fu || (len > 0u && rx == NULL) || !i2c_bus_acquire(config)) {
    return false;
  }

  return i2c_read_bytes(i2c_buses[config->bus].instance, address, rx, len);
#else
  (void)config;
  (void)address;
  (void)rx;
  (void)len;
  return false;
#endif
}

bool i2c_bus_write_register(const i2c_bus_config_t *config, uint8_t address,
                            uint16_t reg,
                            i2c_register_width_t register_width,
                            const uint8_t *tx, size_t len) {
#if I2C_NUM_BUSES > 0
  uint8_t reg_bytes[2];
  size_t reg_len = (size_t)register_width;

  if (address > 0x7Fu || register_width > I2C_REGISTER_16BIT ||
      (len > 0u && tx == NULL) || !i2c_bus_acquire(config)) {
    return false;
  }

  if (register_width == I2C_REGISTER_8BIT) {
    reg_bytes[0] = (uint8_t)reg;
  } else {
    reg_bytes[0] = (uint8_t)(reg >> 8);
    reg_bytes[1] = (uint8_t)reg;
  }

  return i2c_write_bytes(i2c_buses[config->bus].instance, address, reg_bytes,
                         reg_len, tx, len);
#else
  (void)config;
  (void)address;
  (void)reg;
  (void)register_width;
  (void)tx;
  (void)len;
  return false;
#endif
}

bool i2c_bus_read_register(const i2c_bus_config_t *config, uint8_t address,
                           uint16_t reg,
                           i2c_register_width_t register_width, uint8_t *rx,
                           size_t len) {
#if I2C_NUM_BUSES > 0
  i2c_type *instance;
  size_t read = 0;

  if (address > 0x7Fu || register_width > I2C_REGISTER_16BIT ||
      (len > 0u && rx == NULL) || !i2c_bus_acquire(config)) {
    return false;
  }
  if (len == 0u) {
    return true;
  }

  instance = i2c_buses[config->bus].instance;
  if (!i2c_send_register_address(instance, address, reg, register_width)) {
    return false;
  }

  i2c_start_transfer(instance, address, len, I2C_GEN_START_READ);
  while (len > 0u) {
    if (!i2c_wait_for_flag(instance, I2C_RDBF_FLAG, true)) {
      return false;
    }

    *rx++ = i2c_data_receive(instance);
    read++;
    len--;

    if ((read % 255u) == 0u && len > 0u) {
      if (!i2c_wait_for_flag(instance, I2C_TCRLD_FLAG, true)) {
        return false;
      }
      i2c_start_transfer(instance, address, len, I2C_WITHOUT_START);
    }
  }

  if (!i2c_wait_for_flag(instance, I2C_STOPF_FLAG, true)) {
    return false;
  }
  i2c_flag_clear(instance, I2C_STOPF_FLAG);
  i2c_reset_ctrl2(instance);
  return true;
#else
  (void)config;
  (void)address;
  (void)reg;
  (void)register_width;
  (void)rx;
  (void)len;
  return false;
#endif
}
