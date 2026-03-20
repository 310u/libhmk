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

#if I2C_NUM_BUSES > 4
#error "I2C_NUM_BUSES > 4 is not supported"
#endif

#if I2C_NUM_BUSES > 0
#if !defined(I2C_BUS0_INSTANCE) || !defined(I2C_BUS0_CLOCK_ENABLE) ||          \
    !defined(I2C_BUS0_SCL_PORT) || !defined(I2C_BUS0_SCL_PIN) ||               \
    !defined(I2C_BUS0_SDA_PORT) || !defined(I2C_BUS0_SDA_PIN) ||               \
    !defined(I2C_BUS0_PIN_AF)
#error "I2C bus 0 configuration macros are incomplete"
#endif
#endif

#if I2C_NUM_BUSES > 1
#if !defined(I2C_BUS1_INSTANCE) || !defined(I2C_BUS1_CLOCK_ENABLE) ||          \
    !defined(I2C_BUS1_SCL_PORT) || !defined(I2C_BUS1_SCL_PIN) ||               \
    !defined(I2C_BUS1_SDA_PORT) || !defined(I2C_BUS1_SDA_PIN) ||               \
    !defined(I2C_BUS1_PIN_AF)
#error "I2C bus 1 configuration macros are incomplete"
#endif
#endif

#if I2C_NUM_BUSES > 2
#if !defined(I2C_BUS2_INSTANCE) || !defined(I2C_BUS2_CLOCK_ENABLE) ||          \
    !defined(I2C_BUS2_SCL_PORT) || !defined(I2C_BUS2_SCL_PIN) ||               \
    !defined(I2C_BUS2_SDA_PORT) || !defined(I2C_BUS2_SDA_PIN) ||               \
    !defined(I2C_BUS2_PIN_AF)
#error "I2C bus 2 configuration macros are incomplete"
#endif
#endif

#if I2C_NUM_BUSES > 3
#if !defined(I2C_BUS3_INSTANCE) || !defined(I2C_BUS3_CLOCK_ENABLE) ||          \
    !defined(I2C_BUS3_SCL_PORT) || !defined(I2C_BUS3_SCL_PIN) ||               \
    !defined(I2C_BUS3_SDA_PORT) || !defined(I2C_BUS3_SDA_PIN) ||               \
    !defined(I2C_BUS3_PIN_AF)
#error "I2C bus 3 configuration macros are incomplete"
#endif
#endif

#if I2C_NUM_BUSES > 0
static void i2c_enable_gpio_clock(GPIO_TypeDef *port) {
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

typedef struct {
  I2C_TypeDef *instance;
  GPIO_TypeDef *scl_port;
  uint16_t scl_pin;
  GPIO_TypeDef *sda_port;
  uint16_t sda_pin;
  uint32_t pin_af;
  I2C_HandleTypeDef handle;
  bool configured;
  uint32_t last_frequency_hz;
} i2c_bus_state_t;

#define I2C_STM32_BUS_ENTRY(index)                                             \
  {                                                                            \
      .instance = I2C_BUS##index##_INSTANCE,                                   \
      .scl_port = I2C_BUS##index##_SCL_PORT,                                   \
      .scl_pin = I2C_BUS##index##_SCL_PIN,                                     \
      .sda_port = I2C_BUS##index##_SDA_PORT,                                   \
      .sda_pin = I2C_BUS##index##_SDA_PIN,                                     \
      .pin_af = I2C_BUS##index##_PIN_AF,                                       \
      .handle = {0},                                                           \
      .configured = false,                                                     \
      .last_frequency_hz = 0,                                                  \
  }

static i2c_bus_state_t i2c_buses[] = {
#if I2C_NUM_BUSES > 0
    I2C_STM32_BUS_ENTRY(0),
#endif
#if I2C_NUM_BUSES > 1
    I2C_STM32_BUS_ENTRY(1),
#endif
#if I2C_NUM_BUSES > 2
    I2C_STM32_BUS_ENTRY(2),
#endif
#if I2C_NUM_BUSES > 3
    I2C_STM32_BUS_ENTRY(3),
#endif
};

static bool i2c_driver_initialized = false;

static void i2c_enable_bus_clock(uint8_t bus) {
  switch (bus) {
#if I2C_NUM_BUSES > 0
    case 0:
      I2C_BUS0_CLOCK_ENABLE();
      return;
#endif
#if I2C_NUM_BUSES > 1
    case 1:
      I2C_BUS1_CLOCK_ENABLE();
      return;
#endif
#if I2C_NUM_BUSES > 2
    case 2:
      I2C_BUS2_CLOCK_ENABLE();
      return;
#endif
#if I2C_NUM_BUSES > 3
    case 3:
      I2C_BUS3_CLOCK_ENABLE();
      return;
#endif
    default:
      return;
  }
}

static void i2c_configure_af_pin(GPIO_TypeDef *port, uint16_t pin,
                                 uint32_t alternate_function) {
  GPIO_InitTypeDef gpio_init = {0};

  i2c_enable_gpio_clock(port);
  gpio_init.Pin = pin;
  gpio_init.Mode = GPIO_MODE_AF_OD;
  gpio_init.Pull = GPIO_NOPULL;
  gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio_init.Alternate = alternate_function;
  HAL_GPIO_Init(port, &gpio_init);
}

static bool i2c_configure_bus(i2c_bus_state_t *bus_state,
                              const i2c_bus_config_t *config) {
  if (config->frequency_hz == 0u || config->frequency_hz > 400000u) {
    return false;
  }

  if (bus_state->configured &&
      bus_state->last_frequency_hz == config->frequency_hz) {
    return true;
  }

  bus_state->handle.Instance = bus_state->instance;
  bus_state->handle.Init.ClockSpeed = config->frequency_hz;
  bus_state->handle.Init.DutyCycle =
      config->frequency_hz > 100000u ? I2C_DUTYCYCLE_2 : I2C_DUTYCYCLE_16_9;
  bus_state->handle.Init.OwnAddress1 = 0;
  bus_state->handle.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  bus_state->handle.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  bus_state->handle.Init.OwnAddress2 = 0;
  bus_state->handle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  bus_state->handle.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

  (void)HAL_I2C_DeInit(&bus_state->handle);
  if (HAL_I2C_Init(&bus_state->handle) != HAL_OK) {
    return false;
  }

  bus_state->configured = true;
  bus_state->last_frequency_hz = config->frequency_hz;
  return true;
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
    i2c_configure_af_pin(bus_state->scl_port, bus_state->scl_pin,
                         bus_state->pin_af);
    i2c_configure_af_pin(bus_state->sda_port, bus_state->sda_pin,
                         bus_state->pin_af);
    bus_state->handle.Instance = bus_state->instance;
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
  if (address > 0x7Fu || len > UINT16_MAX ||
      (len > 0u && tx == NULL) || !i2c_bus_acquire(config)) {
    return false;
  }
  if (len == 0u) {
    return true;
  }

  return HAL_I2C_Master_Transmit(&i2c_buses[config->bus].handle,
                                 (uint16_t)(address << 1), (uint8_t *)tx,
                                 (uint16_t)len, I2C_TRANSFER_TIMEOUT_MS) ==
         HAL_OK;
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
  if (address > 0x7Fu || len > UINT16_MAX ||
      (len > 0u && rx == NULL) || !i2c_bus_acquire(config)) {
    return false;
  }
  if (len == 0u) {
    return true;
  }

  return HAL_I2C_Master_Receive(&i2c_buses[config->bus].handle,
                                (uint16_t)(address << 1), rx, (uint16_t)len,
                                I2C_TRANSFER_TIMEOUT_MS) == HAL_OK;
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
  uint8_t prefix[2];
  uint16_t mem_size;

  if (address > 0x7Fu || len > UINT16_MAX ||
      register_width > I2C_REGISTER_16BIT ||
      (len > 0u && tx == NULL) || !i2c_bus_acquire(config)) {
    return false;
  }

  mem_size = register_width == I2C_REGISTER_16BIT ? I2C_MEMADD_SIZE_16BIT
                                                  : I2C_MEMADD_SIZE_8BIT;
  if (len == 0u) {
    if (register_width == I2C_REGISTER_16BIT) {
      prefix[0] = (uint8_t)(reg >> 8);
      prefix[1] = (uint8_t)reg;
      return HAL_I2C_Master_Transmit(&i2c_buses[config->bus].handle,
                                     (uint16_t)(address << 1), prefix, 2u,
                                     I2C_TRANSFER_TIMEOUT_MS) == HAL_OK;
    }
    prefix[0] = (uint8_t)reg;
    return HAL_I2C_Master_Transmit(&i2c_buses[config->bus].handle,
                                   (uint16_t)(address << 1), prefix, 1u,
                                   I2C_TRANSFER_TIMEOUT_MS) == HAL_OK;
  }

  return HAL_I2C_Mem_Write(&i2c_buses[config->bus].handle,
                           (uint16_t)(address << 1), reg, mem_size,
                           (uint8_t *)tx, (uint16_t)len,
                           I2C_TRANSFER_TIMEOUT_MS) == HAL_OK;
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
  uint16_t mem_size;

  if (address > 0x7Fu || len > UINT16_MAX ||
      register_width > I2C_REGISTER_16BIT ||
      (len > 0u && rx == NULL) || !i2c_bus_acquire(config)) {
    return false;
  }
  if (len == 0u) {
    return true;
  }

  mem_size = register_width == I2C_REGISTER_16BIT ? I2C_MEMADD_SIZE_16BIT
                                                  : I2C_MEMADD_SIZE_8BIT;
  return HAL_I2C_Mem_Read(&i2c_buses[config->bus].handle,
                          (uint16_t)(address << 1), reg, mem_size, rx,
                          (uint16_t)len, I2C_TRANSFER_TIMEOUT_MS) == HAL_OK;
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
