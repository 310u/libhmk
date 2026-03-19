#include <string.h>
#include <unity.h>

#include "hardware/rgb_api.h"
#include "stm32f4xx_hal.h"

GPIO_TypeDef mock_gpioa;
uint32_t mock_gpio_clock_mask;
GPIO_InitTypeDef last_gpio_init;
GPIO_TypeDef *last_gpio_init_port;
uint32_t mock_cycle_counter;
uint32_t mock_primask_value;
uint32_t mock_disable_irq_count;
uint32_t mock_enable_irq_count;

uint32_t board_cycle_count(void) {
  mock_cycle_counter += 64u;
  return mock_cycle_counter;
}

void HAL_GPIO_Init(GPIO_TypeDef *GPIOx, GPIO_InitTypeDef *GPIO_Init) {
  last_gpio_init_port = GPIOx;
  last_gpio_init = *GPIO_Init;
}

void setUp(void) {
  memset(&mock_gpioa, 0, sizeof(mock_gpioa));
  mock_gpio_clock_mask = 0;
  memset(&last_gpio_init, 0, sizeof(last_gpio_init));
  last_gpio_init_port = NULL;
  mock_cycle_counter = 0;
  mock_primask_value = 0;
  mock_disable_irq_count = 0;
  mock_enable_irq_count = 0;
}

void tearDown(void) {}

void test_stm32_rgb_driver_init_configures_output_pin(void) {
  rgb_driver_init();

  TEST_ASSERT_EQUAL_HEX32(1u, mock_gpio_clock_mask);
  TEST_ASSERT_EQUAL_PTR(GPIOA, last_gpio_init_port);
  TEST_ASSERT_EQUAL_HEX32(GPIO_PIN_8, last_gpio_init.Pin);
  TEST_ASSERT_EQUAL_UINT32(GPIO_MODE_OUTPUT_PP, last_gpio_init.Mode);
  TEST_ASSERT_EQUAL_UINT32(GPIO_NOPULL, last_gpio_init.Pull);
  TEST_ASSERT_EQUAL_UINT32(GPIO_SPEED_FREQ_VERY_HIGH, last_gpio_init.Speed);
  TEST_ASSERT_EQUAL_HEX32((uint32_t)GPIO_PIN_8 << 16, mock_gpioa.BSRR);
}

void test_stm32_rgb_driver_write_restores_interrupt_state(void) {
  static const uint8_t frame[] = {0xA5};

  rgb_driver_write(frame, sizeof(frame));

  TEST_ASSERT_EQUAL_UINT32(1u, mock_disable_irq_count);
  TEST_ASSERT_EQUAL_UINT32(1u, mock_enable_irq_count);
  TEST_ASSERT_EQUAL_HEX32((uint32_t)GPIO_PIN_8 << 16, mock_gpioa.BSRR);
}

void test_stm32_rgb_driver_write_preserves_pre_disabled_irq_state(void) {
  static const uint8_t frame[] = {0xFF};

  mock_primask_value = 1u;
  rgb_driver_write(frame, sizeof(frame));

  TEST_ASSERT_EQUAL_UINT32(1u, mock_disable_irq_count);
  TEST_ASSERT_EQUAL_UINT32(0u, mock_enable_irq_count);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_stm32_rgb_driver_init_configures_output_pin);
  RUN_TEST(test_stm32_rgb_driver_write_restores_interrupt_state);
  RUN_TEST(test_stm32_rgb_driver_write_preserves_pre_disabled_irq_state);
  return UNITY_END();
}
