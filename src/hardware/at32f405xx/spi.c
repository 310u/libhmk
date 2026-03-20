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

#if SPI_NUM_BUSES > 4
#error "SPI_NUM_BUSES > 4 is not supported"
#endif

#if SPI_NUM_BUSES > 0
#if !defined(SPI_BUS0_INSTANCE) || !defined(SPI_BUS0_CLOCK) ||                 \
    !defined(SPI_BUS0_CLOCK_HZ) || !defined(SPI_BUS0_SCK_PORT) ||              \
    !defined(SPI_BUS0_SCK_PIN) || !defined(SPI_BUS0_SCK_PIN_SOURCE) ||         \
    !defined(SPI_BUS0_PIN_MUX)
#error "SPI bus 0 configuration macros are incomplete"
#endif
#if !defined(SPI_BUS0_MISO_PORT)
#define SPI_BUS0_MISO_PORT NULL
#define SPI_BUS0_MISO_PIN 0
#define SPI_BUS0_MISO_PIN_SOURCE 0
#endif
#if !defined(SPI_BUS0_MOSI_PORT)
#define SPI_BUS0_MOSI_PORT NULL
#define SPI_BUS0_MOSI_PIN 0
#define SPI_BUS0_MOSI_PIN_SOURCE 0
#endif
#endif

#if SPI_NUM_BUSES > 1
#if !defined(SPI_BUS1_INSTANCE) || !defined(SPI_BUS1_CLOCK) ||                 \
    !defined(SPI_BUS1_CLOCK_HZ) || !defined(SPI_BUS1_SCK_PORT) ||              \
    !defined(SPI_BUS1_SCK_PIN) || !defined(SPI_BUS1_SCK_PIN_SOURCE) ||         \
    !defined(SPI_BUS1_PIN_MUX)
#error "SPI bus 1 configuration macros are incomplete"
#endif
#if !defined(SPI_BUS1_MISO_PORT)
#define SPI_BUS1_MISO_PORT NULL
#define SPI_BUS1_MISO_PIN 0
#define SPI_BUS1_MISO_PIN_SOURCE 0
#endif
#if !defined(SPI_BUS1_MOSI_PORT)
#define SPI_BUS1_MOSI_PORT NULL
#define SPI_BUS1_MOSI_PIN 0
#define SPI_BUS1_MOSI_PIN_SOURCE 0
#endif
#endif

#if SPI_NUM_BUSES > 2
#if !defined(SPI_BUS2_INSTANCE) || !defined(SPI_BUS2_CLOCK) ||                 \
    !defined(SPI_BUS2_CLOCK_HZ) || !defined(SPI_BUS2_SCK_PORT) ||              \
    !defined(SPI_BUS2_SCK_PIN) || !defined(SPI_BUS2_SCK_PIN_SOURCE) ||         \
    !defined(SPI_BUS2_PIN_MUX)
#error "SPI bus 2 configuration macros are incomplete"
#endif
#if !defined(SPI_BUS2_MISO_PORT)
#define SPI_BUS2_MISO_PORT NULL
#define SPI_BUS2_MISO_PIN 0
#define SPI_BUS2_MISO_PIN_SOURCE 0
#endif
#if !defined(SPI_BUS2_MOSI_PORT)
#define SPI_BUS2_MOSI_PORT NULL
#define SPI_BUS2_MOSI_PIN 0
#define SPI_BUS2_MOSI_PIN_SOURCE 0
#endif
#endif

#if SPI_NUM_BUSES > 3
#if !defined(SPI_BUS3_INSTANCE) || !defined(SPI_BUS3_CLOCK) ||                 \
    !defined(SPI_BUS3_CLOCK_HZ) || !defined(SPI_BUS3_SCK_PORT) ||              \
    !defined(SPI_BUS3_SCK_PIN) || !defined(SPI_BUS3_SCK_PIN_SOURCE) ||         \
    !defined(SPI_BUS3_PIN_MUX)
#error "SPI bus 3 configuration macros are incomplete"
#endif
#if !defined(SPI_BUS3_MISO_PORT)
#define SPI_BUS3_MISO_PORT NULL
#define SPI_BUS3_MISO_PIN 0
#define SPI_BUS3_MISO_PIN_SOURCE 0
#endif
#if !defined(SPI_BUS3_MOSI_PORT)
#define SPI_BUS3_MOSI_PORT NULL
#define SPI_BUS3_MOSI_PIN 0
#define SPI_BUS3_MOSI_PIN_SOURCE 0
#endif
#endif

static void spi_enable_gpio_clock(gpio_type *port) {
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

#if SPI_NUM_BUSES > 0
typedef struct {
  spi_type *instance;
  uint32_t clock_hz;
  gpio_type *sck_port;
  uint16_t sck_pin;
  uint16_t sck_pin_source;
  gpio_type *miso_port;
  uint16_t miso_pin;
  uint16_t miso_pin_source;
  gpio_type *mosi_port;
  uint16_t mosi_pin;
  uint16_t mosi_pin_source;
  uint16_t pin_mux;
  bool configured;
  uint32_t last_frequency_hz;
  spi_bus_mode_t last_mode;
  bool last_lsb_first;
} spi_bus_state_t;

#define SPI_AT32_BUS_ENTRY(index)                                              \
  {                                                                            \
      .instance = SPI_BUS##index##_INSTANCE,                                   \
      .clock_hz = SPI_BUS##index##_CLOCK_HZ,                                   \
      .sck_port = SPI_BUS##index##_SCK_PORT,                                   \
      .sck_pin = SPI_BUS##index##_SCK_PIN,                                     \
      .sck_pin_source = SPI_BUS##index##_SCK_PIN_SOURCE,                       \
      .miso_port = SPI_BUS##index##_MISO_PORT,                                 \
      .miso_pin = SPI_BUS##index##_MISO_PIN,                                   \
      .miso_pin_source = SPI_BUS##index##_MISO_PIN_SOURCE,                     \
      .mosi_port = SPI_BUS##index##_MOSI_PORT,                                 \
      .mosi_pin = SPI_BUS##index##_MOSI_PIN,                                   \
      .mosi_pin_source = SPI_BUS##index##_MOSI_PIN_SOURCE,                     \
      .pin_mux = SPI_BUS##index##_PIN_MUX,                                     \
      .configured = false,                                                     \
      .last_frequency_hz = 0,                                                  \
      .last_mode = SPI_BUS_MODE_0,                                             \
      .last_lsb_first = false,                                                 \
  }

static spi_bus_state_t spi_buses[] = {
#if SPI_NUM_BUSES > 0
    SPI_AT32_BUS_ENTRY(0),
#endif
#if SPI_NUM_BUSES > 1
    SPI_AT32_BUS_ENTRY(1),
#endif
#if SPI_NUM_BUSES > 2
    SPI_AT32_BUS_ENTRY(2),
#endif
#if SPI_NUM_BUSES > 3
    SPI_AT32_BUS_ENTRY(3),
#endif
};

static bool spi_driver_initialized = false;

static void spi_enable_bus_clock(uint8_t bus) {
  switch (bus) {
#if SPI_NUM_BUSES > 0
    case 0:
      crm_periph_clock_enable(SPI_BUS0_CLOCK, TRUE);
      return;
#endif
#if SPI_NUM_BUSES > 1
    case 1:
      crm_periph_clock_enable(SPI_BUS1_CLOCK, TRUE);
      return;
#endif
#if SPI_NUM_BUSES > 2
    case 2:
      crm_periph_clock_enable(SPI_BUS2_CLOCK, TRUE);
      return;
#endif
#if SPI_NUM_BUSES > 3
    case 3:
      crm_periph_clock_enable(SPI_BUS3_CLOCK, TRUE);
      return;
#endif
    default:
      return;
  }
}

static void spi_configure_mux_pin(gpio_type *port, uint16_t pin,
                                  uint16_t pin_source, uint16_t pin_mux) {
  gpio_init_type gpio_init_struct;

  if (port == NULL || pin == 0u) {
    return;
  }

  spi_enable_gpio_clock(port);
  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_pins = pin;
  gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init(port, &gpio_init_struct);
  gpio_pin_mux_config(port, pin_source, pin_mux);
}

static spi_mclk_freq_div_type spi_pick_divider(uint32_t clock_hz,
                                               uint32_t target_hz) {
  if (target_hz == 0u || target_hz >= (clock_hz / 2u)) {
    return SPI_MCLK_DIV_2;
  }
  if (target_hz >= (clock_hz / 4u)) {
    return SPI_MCLK_DIV_4;
  }
  if (target_hz >= (clock_hz / 8u)) {
    return SPI_MCLK_DIV_8;
  }
  if (target_hz >= (clock_hz / 16u)) {
    return SPI_MCLK_DIV_16;
  }
  if (target_hz >= (clock_hz / 32u)) {
    return SPI_MCLK_DIV_32;
  }
  if (target_hz >= (clock_hz / 64u)) {
    return SPI_MCLK_DIV_64;
  }
  if (target_hz >= (clock_hz / 128u)) {
    return SPI_MCLK_DIV_128;
  }
  if (target_hz >= (clock_hz / 256u)) {
    return SPI_MCLK_DIV_256;
  }
  if (target_hz >= (clock_hz / 512u)) {
    return SPI_MCLK_DIV_512;
  }
  return SPI_MCLK_DIV_1024;
}

static bool spi_configure_bus(spi_bus_state_t *bus_state,
                              const spi_bus_config_t *config) {
  spi_init_type spi_init_struct;

  if (config->mode > SPI_BUS_MODE_3 || config->frequency_hz == 0u) {
    return false;
  }

  if (bus_state->configured &&
      bus_state->last_frequency_hz == config->frequency_hz &&
      bus_state->last_mode == config->mode &&
      bus_state->last_lsb_first == config->lsb_first) {
    return true;
  }

  spi_default_para_init(&spi_init_struct);
  spi_init_struct.transmission_mode = SPI_TRANSMIT_FULL_DUPLEX;
  spi_init_struct.master_slave_mode = SPI_MODE_MASTER;
  spi_init_struct.mclk_freq_division =
      spi_pick_divider(bus_state->clock_hz, config->frequency_hz);
  spi_init_struct.first_bit_transmission =
      config->lsb_first ? SPI_FIRST_BIT_LSB : SPI_FIRST_BIT_MSB;
  spi_init_struct.frame_bit_num = SPI_FRAME_8BIT;
  spi_init_struct.clock_polarity =
      (config->mode == SPI_BUS_MODE_2 || config->mode == SPI_BUS_MODE_3)
          ? SPI_CLOCK_POLARITY_HIGH
          : SPI_CLOCK_POLARITY_LOW;
  spi_init_struct.clock_phase =
      (config->mode == SPI_BUS_MODE_1 || config->mode == SPI_BUS_MODE_3)
          ? SPI_CLOCK_PHASE_2EDGE
          : SPI_CLOCK_PHASE_1EDGE;
  spi_init_struct.cs_mode_selection = SPI_CS_SOFTWARE_MODE;

  spi_enable(bus_state->instance, FALSE);
  spi_init(bus_state->instance, &spi_init_struct);
  spi_software_cs_internal_level_set(bus_state->instance,
                                     SPI_SWCS_INTERNAL_LEVEL_HIGHT);
  spi_enable(bus_state->instance, TRUE);

  bus_state->configured = true;
  bus_state->last_frequency_hz = config->frequency_hz;
  bus_state->last_mode = config->mode;
  bus_state->last_lsb_first = config->lsb_first;
  return true;
}
#endif

void spi_bus_init(void) {
#if SPI_NUM_BUSES > 0
  if (spi_driver_initialized) {
    return;
  }

  for (uint8_t bus = 0; bus < M_ARRAY_SIZE(spi_buses); bus++) {
    const spi_bus_state_t *bus_state = &spi_buses[bus];
    spi_enable_bus_clock(bus);
    spi_configure_mux_pin(bus_state->sck_port, bus_state->sck_pin,
                          bus_state->sck_pin_source, bus_state->pin_mux);
    spi_configure_mux_pin(bus_state->miso_port, bus_state->miso_pin,
                          bus_state->miso_pin_source, bus_state->pin_mux);
    spi_configure_mux_pin(bus_state->mosi_port, bus_state->mosi_pin,
                          bus_state->mosi_pin_source, bus_state->pin_mux);
  }

  spi_driver_initialized = true;
#endif
}

bool spi_bus_acquire(const spi_bus_config_t *config) {
#if SPI_NUM_BUSES > 0
  if (config == NULL || config->bus >= M_ARRAY_SIZE(spi_buses)) {
    return false;
  }

  if (!spi_driver_initialized) {
    spi_bus_init();
  }

  return spi_configure_bus(&spi_buses[config->bus], config);
#else
  (void)config;
  return false;
#endif
}

void spi_bus_release(const spi_bus_config_t *config) { (void)config; }

bool spi_bus_transfer(const spi_bus_config_t *config, const uint8_t *tx,
                      uint8_t *rx, size_t len) {
#if SPI_NUM_BUSES > 0
  spi_type *instance;

  if (len == 0u) {
    return true;
  }

  if (!spi_bus_acquire(config)) {
    return false;
  }

  instance = spi_buses[config->bus].instance;
  for (size_t i = 0; i < len; i++) {
    uint16_t tx_word = tx != NULL ? tx[i] : 0xFFu;
    while (spi_i2s_flag_get(instance, SPI_I2S_TDBE_FLAG) == RESET) {
    }
    spi_i2s_data_transmit(instance, tx_word);
    while (spi_i2s_flag_get(instance, SPI_I2S_RDBF_FLAG) == RESET) {
    }
    if (rx != NULL) {
      rx[i] = (uint8_t)spi_i2s_data_receive(instance);
    } else {
      (void)spi_i2s_data_receive(instance);
    }
  }

  return true;
#else
  (void)config;
  (void)tx;
  (void)rx;
  (void)len;
  return false;
#endif
}

void spi_cs_init(const spi_chip_select_t *chip_select) {
  gpio_init_type gpio_init_struct;
  gpio_type *port;
  uint16_t pin;

  if (chip_select == NULL || chip_select->port == NULL || chip_select->pin == 0u) {
    return;
  }

  port = (gpio_type *)chip_select->port;
  pin = (uint16_t)chip_select->pin;

  spi_enable_gpio_clock(port);
  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_pins = pin;
  gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init(port, &gpio_init_struct);
  spi_cs_deselect(chip_select);
}

void spi_cs_select(const spi_chip_select_t *chip_select) {
  gpio_type *port;
  uint16_t pin;

  if (chip_select == NULL || chip_select->port == NULL || chip_select->pin == 0u) {
    return;
  }

  port = (gpio_type *)chip_select->port;
  pin = (uint16_t)chip_select->pin;
  if (chip_select->active_low) {
    port->clr = pin;
  } else {
    port->scr = pin;
  }
}

void spi_cs_deselect(const spi_chip_select_t *chip_select) {
  gpio_type *port;
  uint16_t pin;

  if (chip_select == NULL || chip_select->port == NULL || chip_select->pin == 0u) {
    return;
  }

  port = (gpio_type *)chip_select->port;
  pin = (uint16_t)chip_select->pin;
  if (chip_select->active_low) {
    port->scr = pin;
  } else {
    port->clr = pin;
  }
}
