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

#include "stm32f4xx_hal.h"

#if SPI_NUM_BUSES > 4
#error "SPI_NUM_BUSES > 4 is not supported"
#endif

#if SPI_NUM_BUSES > 0
#if !defined(SPI_BUS0_INSTANCE) || !defined(SPI_BUS0_CLOCK_ENABLE) ||          \
    !defined(SPI_BUS0_CLOCK_HZ) || !defined(SPI_BUS0_SCK_PORT) ||              \
    !defined(SPI_BUS0_SCK_PIN) || !defined(SPI_BUS0_PIN_AF)
#error "SPI bus 0 configuration macros are incomplete"
#endif
#if !defined(SPI_BUS0_MISO_PORT)
#define SPI_BUS0_MISO_PORT NULL
#define SPI_BUS0_MISO_PIN 0
#endif
#if !defined(SPI_BUS0_MOSI_PORT)
#define SPI_BUS0_MOSI_PORT NULL
#define SPI_BUS0_MOSI_PIN 0
#endif
#endif

#if SPI_NUM_BUSES > 1
#if !defined(SPI_BUS1_INSTANCE) || !defined(SPI_BUS1_CLOCK_ENABLE) ||          \
    !defined(SPI_BUS1_CLOCK_HZ) || !defined(SPI_BUS1_SCK_PORT) ||              \
    !defined(SPI_BUS1_SCK_PIN) || !defined(SPI_BUS1_PIN_AF)
#error "SPI bus 1 configuration macros are incomplete"
#endif
#if !defined(SPI_BUS1_MISO_PORT)
#define SPI_BUS1_MISO_PORT NULL
#define SPI_BUS1_MISO_PIN 0
#endif
#if !defined(SPI_BUS1_MOSI_PORT)
#define SPI_BUS1_MOSI_PORT NULL
#define SPI_BUS1_MOSI_PIN 0
#endif
#endif

#if SPI_NUM_BUSES > 2
#if !defined(SPI_BUS2_INSTANCE) || !defined(SPI_BUS2_CLOCK_ENABLE) ||          \
    !defined(SPI_BUS2_CLOCK_HZ) || !defined(SPI_BUS2_SCK_PORT) ||              \
    !defined(SPI_BUS2_SCK_PIN) || !defined(SPI_BUS2_PIN_AF)
#error "SPI bus 2 configuration macros are incomplete"
#endif
#if !defined(SPI_BUS2_MISO_PORT)
#define SPI_BUS2_MISO_PORT NULL
#define SPI_BUS2_MISO_PIN 0
#endif
#if !defined(SPI_BUS2_MOSI_PORT)
#define SPI_BUS2_MOSI_PORT NULL
#define SPI_BUS2_MOSI_PIN 0
#endif
#endif

#if SPI_NUM_BUSES > 3
#if !defined(SPI_BUS3_INSTANCE) || !defined(SPI_BUS3_CLOCK_ENABLE) ||          \
    !defined(SPI_BUS3_CLOCK_HZ) || !defined(SPI_BUS3_SCK_PORT) ||              \
    !defined(SPI_BUS3_SCK_PIN) || !defined(SPI_BUS3_PIN_AF)
#error "SPI bus 3 configuration macros are incomplete"
#endif
#if !defined(SPI_BUS3_MISO_PORT)
#define SPI_BUS3_MISO_PORT NULL
#define SPI_BUS3_MISO_PIN 0
#endif
#if !defined(SPI_BUS3_MOSI_PORT)
#define SPI_BUS3_MOSI_PORT NULL
#define SPI_BUS3_MOSI_PIN 0
#endif
#endif

static void spi_enable_gpio_clock(GPIO_TypeDef *port) {
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
#if defined(GPIOG)
  if (port == GPIOG) {
    __HAL_RCC_GPIOG_CLK_ENABLE();
    return;
  }
#endif
#if defined(GPIOH)
  if (port == GPIOH) {
    __HAL_RCC_GPIOH_CLK_ENABLE();
    return;
  }
#endif
#if defined(GPIOI)
  if (port == GPIOI) {
    __HAL_RCC_GPIOI_CLK_ENABLE();
    return;
  }
#endif
}

#if SPI_NUM_BUSES > 0
typedef struct {
  SPI_TypeDef *instance;
  uint32_t clock_hz;
  GPIO_TypeDef *sck_port;
  uint16_t sck_pin;
  GPIO_TypeDef *miso_port;
  uint16_t miso_pin;
  GPIO_TypeDef *mosi_port;
  uint16_t mosi_pin;
  uint32_t pin_af;
  SPI_HandleTypeDef handle;
  bool configured;
  uint32_t last_frequency_hz;
  spi_bus_mode_t last_mode;
  bool last_lsb_first;
} spi_bus_state_t;

#define SPI_STM32_BUS_ENTRY(index)                                             \
  {                                                                            \
      .instance = SPI_BUS##index##_INSTANCE,                                   \
      .clock_hz = SPI_BUS##index##_CLOCK_HZ,                                   \
      .sck_port = SPI_BUS##index##_SCK_PORT,                                   \
      .sck_pin = SPI_BUS##index##_SCK_PIN,                                     \
      .miso_port = SPI_BUS##index##_MISO_PORT,                                 \
      .miso_pin = SPI_BUS##index##_MISO_PIN,                                   \
      .mosi_port = SPI_BUS##index##_MOSI_PORT,                                 \
      .mosi_pin = SPI_BUS##index##_MOSI_PIN,                                   \
      .pin_af = SPI_BUS##index##_PIN_AF,                                       \
      .handle = {0},                                                           \
      .configured = false,                                                     \
      .last_frequency_hz = 0,                                                  \
      .last_mode = SPI_BUS_MODE_0,                                             \
      .last_lsb_first = false,                                                 \
  }

static spi_bus_state_t spi_buses[] = {
#if SPI_NUM_BUSES > 0
    SPI_STM32_BUS_ENTRY(0),
#endif
#if SPI_NUM_BUSES > 1
    SPI_STM32_BUS_ENTRY(1),
#endif
#if SPI_NUM_BUSES > 2
    SPI_STM32_BUS_ENTRY(2),
#endif
#if SPI_NUM_BUSES > 3
    SPI_STM32_BUS_ENTRY(3),
#endif
};

static bool spi_driver_initialized = false;

static void spi_enable_bus_clock(uint8_t bus) {
  switch (bus) {
#if SPI_NUM_BUSES > 0
    case 0:
      SPI_BUS0_CLOCK_ENABLE();
      return;
#endif
#if SPI_NUM_BUSES > 1
    case 1:
      SPI_BUS1_CLOCK_ENABLE();
      return;
#endif
#if SPI_NUM_BUSES > 2
    case 2:
      SPI_BUS2_CLOCK_ENABLE();
      return;
#endif
#if SPI_NUM_BUSES > 3
    case 3:
      SPI_BUS3_CLOCK_ENABLE();
      return;
#endif
    default:
      return;
  }
}

static void spi_configure_af_pin(GPIO_TypeDef *port, uint16_t pin,
                                 uint32_t alternate_function) {
  GPIO_InitTypeDef gpio_init = {0};

  if (port == NULL || pin == 0u) {
    return;
  }

  spi_enable_gpio_clock(port);
  gpio_init.Pin = pin;
  gpio_init.Mode = GPIO_MODE_AF_PP;
  gpio_init.Pull = GPIO_NOPULL;
  gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio_init.Alternate = alternate_function;
  HAL_GPIO_Init(port, &gpio_init);
}

static uint32_t spi_pick_prescaler(uint32_t clock_hz, uint32_t target_hz) {
  if (target_hz == 0u || target_hz >= (clock_hz / 2u)) {
    return SPI_BAUDRATEPRESCALER_2;
  }
  if (target_hz >= (clock_hz / 4u)) {
    return SPI_BAUDRATEPRESCALER_4;
  }
  if (target_hz >= (clock_hz / 8u)) {
    return SPI_BAUDRATEPRESCALER_8;
  }
  if (target_hz >= (clock_hz / 16u)) {
    return SPI_BAUDRATEPRESCALER_16;
  }
  if (target_hz >= (clock_hz / 32u)) {
    return SPI_BAUDRATEPRESCALER_32;
  }
  if (target_hz >= (clock_hz / 64u)) {
    return SPI_BAUDRATEPRESCALER_64;
  }
  if (target_hz >= (clock_hz / 128u)) {
    return SPI_BAUDRATEPRESCALER_128;
  }
  return SPI_BAUDRATEPRESCALER_256;
}

static bool spi_configure_bus(spi_bus_state_t *bus_state,
                              const spi_bus_config_t *config) {
  if (config->mode > SPI_BUS_MODE_3 || config->frequency_hz == 0u) {
    return false;
  }

  if (bus_state->configured &&
      bus_state->last_frequency_hz == config->frequency_hz &&
      bus_state->last_mode == config->mode &&
      bus_state->last_lsb_first == config->lsb_first) {
    return true;
  }

  bus_state->handle.Instance = bus_state->instance;
  bus_state->handle.Init.Mode = SPI_MODE_MASTER;
  bus_state->handle.Init.Direction = SPI_DIRECTION_2LINES;
  bus_state->handle.Init.DataSize = SPI_DATASIZE_8BIT;
  bus_state->handle.Init.CLKPolarity =
      (config->mode == SPI_BUS_MODE_2 || config->mode == SPI_BUS_MODE_3)
          ? SPI_POLARITY_HIGH
          : SPI_POLARITY_LOW;
  bus_state->handle.Init.CLKPhase =
      (config->mode == SPI_BUS_MODE_1 || config->mode == SPI_BUS_MODE_3)
          ? SPI_PHASE_2EDGE
          : SPI_PHASE_1EDGE;
  bus_state->handle.Init.NSS = SPI_NSS_SOFT;
  bus_state->handle.Init.BaudRatePrescaler =
      spi_pick_prescaler(bus_state->clock_hz, config->frequency_hz);
  bus_state->handle.Init.FirstBit =
      config->lsb_first ? SPI_FIRSTBIT_LSB : SPI_FIRSTBIT_MSB;
  bus_state->handle.Init.TIMode = SPI_TIMODE_DISABLE;
  bus_state->handle.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  bus_state->handle.Init.CRCPolynomial = 7;

  (void)HAL_SPI_DeInit(&bus_state->handle);
  if (HAL_SPI_Init(&bus_state->handle) != HAL_OK) {
    return false;
  }

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
    spi_bus_state_t *bus_state = &spi_buses[bus];
    spi_enable_bus_clock(bus);
    spi_configure_af_pin(bus_state->sck_port, bus_state->sck_pin,
                         bus_state->pin_af);
    spi_configure_af_pin(bus_state->miso_port, bus_state->miso_pin,
                         bus_state->pin_af);
    spi_configure_af_pin(bus_state->mosi_port, bus_state->mosi_pin,
                         bus_state->pin_af);
    bus_state->handle.Instance = bus_state->instance;
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
  SPI_HandleTypeDef *handle;

  if (len == 0u) {
    return true;
  }

  if (!spi_bus_acquire(config)) {
    return false;
  }

  handle = &spi_buses[config->bus].handle;
  for (size_t i = 0; i < len; i++) {
    uint8_t tx_byte = tx != NULL ? tx[i] : 0xFFu;
    uint8_t rx_byte = 0;
    if (HAL_SPI_TransmitReceive(handle, &tx_byte, &rx_byte, 1u,
                                HAL_MAX_DELAY) != HAL_OK) {
      return false;
    }
    if (rx != NULL) {
      rx[i] = rx_byte;
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
  GPIO_InitTypeDef gpio_init = {0};
  GPIO_TypeDef *port;
  uint16_t pin;

  if (chip_select == NULL || chip_select->port == NULL || chip_select->pin == 0u) {
    return;
  }

  port = (GPIO_TypeDef *)chip_select->port;
  pin = (uint16_t)chip_select->pin;
  spi_enable_gpio_clock(port);
  gpio_init.Pin = pin;
  gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
  gpio_init.Pull = GPIO_NOPULL;
  gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(port, &gpio_init);
  spi_cs_deselect(chip_select);
}

void spi_cs_select(const spi_chip_select_t *chip_select) {
  GPIO_TypeDef *port;
  uint16_t pin;

  if (chip_select == NULL || chip_select->port == NULL || chip_select->pin == 0u) {
    return;
  }

  port = (GPIO_TypeDef *)chip_select->port;
  pin = (uint16_t)chip_select->pin;
  if (chip_select->active_low) {
    port->BSRR = (uint32_t)pin << 16u;
  } else {
    port->BSRR = pin;
  }
}

void spi_cs_deselect(const spi_chip_select_t *chip_select) {
  GPIO_TypeDef *port;
  uint16_t pin;

  if (chip_select == NULL || chip_select->port == NULL || chip_select->pin == 0u) {
    return;
  }

  port = (GPIO_TypeDef *)chip_select->port;
  pin = (uint16_t)chip_select->pin;
  if (chip_select->active_low) {
    port->BSRR = pin;
  } else {
    port->BSRR = (uint32_t)pin << 16u;
  }
}
