#include <unity.h>

#include "stm32f4xx_hal.h"
#include "encoder.h"
#include "layout.h"

GPIO_TypeDef mock_gpioa = {0};
GPIO_TypeDef mock_gpiob = {0};
GPIO_TypeDef mock_gpioc = {0};

static GPIO_PinState gpio_a0_state;
static GPIO_PinState gpio_a1_state;
static uint8_t processed_keys[8];
static bool processed_pressed[8];
static uint8_t process_count;
static uint8_t gpio_init_count;

static void set_encoder_pins(uint8_t state) {
  gpio_a0_state = (state & 0x01u) ? GPIO_PIN_SET : GPIO_PIN_RESET;
  gpio_a1_state = (state & 0x02u) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}

GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin) {
  if (port == GPIOA && pin == GPIO_PIN_0) {
    return gpio_a0_state;
  }
  if (port == GPIOA && pin == GPIO_PIN_1) {
    return gpio_a1_state;
  }
  return GPIO_PIN_RESET;
}

void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *init) {
  (void)port;
  (void)init;
  gpio_init_count++;
}

bool layout_process_key(uint8_t key, bool pressed) {
  if (process_count < M_ARRAY_SIZE(processed_keys)) {
    processed_keys[process_count] = key;
    processed_pressed[process_count] = pressed;
    process_count++;
  }
  return key != 0xffu;
}

void setUp(void) {
  set_encoder_pins(0u);
  memset(processed_keys, 0, sizeof(processed_keys));
  memset(processed_pressed, 0, sizeof(processed_pressed));
  process_count = 0;
  gpio_init_count = 0;
}

void tearDown(void) {}

void test_encoder_init_configures_phase_inputs(void) {
  encoder_init();

  TEST_ASSERT_EQUAL_UINT8(2, gpio_init_count);
}

void test_encoder_emits_clockwise_tap(void) {
  encoder_init();

  set_encoder_pins(0x02u);
  encoder_task();
  set_encoder_pins(0x03u);
  encoder_task();
  set_encoder_pins(0x01u);
  encoder_task();
  set_encoder_pins(0x00u);
  encoder_task();

  TEST_ASSERT_EQUAL_UINT8(1, process_count);
  TEST_ASSERT_EQUAL_UINT8(4, processed_keys[0]);
  TEST_ASSERT_TRUE(processed_pressed[0]);

  encoder_task();

  TEST_ASSERT_EQUAL_UINT8(2, process_count);
  TEST_ASSERT_EQUAL_UINT8(4, processed_keys[1]);
  TEST_ASSERT_FALSE(processed_pressed[1]);
}

void test_encoder_queues_repeated_steps_until_previous_release(void) {
  encoder_init();

  const uint8_t clockwise_sequence[] = {0x02u, 0x03u, 0x01u, 0x00u,
                                        0x02u, 0x03u, 0x01u, 0x00u};

  for (uint8_t i = 0; i < M_ARRAY_SIZE(clockwise_sequence); i++) {
    set_encoder_pins(clockwise_sequence[i]);
    encoder_task();
  }

  TEST_ASSERT_EQUAL_UINT8(3, process_count);
  TEST_ASSERT_EQUAL_UINT8(4, processed_keys[0]);
  TEST_ASSERT_TRUE(processed_pressed[0]);
  TEST_ASSERT_EQUAL_UINT8(4, processed_keys[1]);
  TEST_ASSERT_FALSE(processed_pressed[1]);
  TEST_ASSERT_EQUAL_UINT8(4, processed_keys[2]);
  TEST_ASSERT_TRUE(processed_pressed[2]);

  encoder_task();

  TEST_ASSERT_EQUAL_UINT8(4, process_count);
  TEST_ASSERT_EQUAL_UINT8(4, processed_keys[3]);
  TEST_ASSERT_FALSE(processed_pressed[3]);
}

void test_encoder_emits_counterclockwise_tap(void) {
  encoder_init();

  set_encoder_pins(0x01u);
  encoder_task();
  set_encoder_pins(0x03u);
  encoder_task();
  set_encoder_pins(0x02u);
  encoder_task();
  set_encoder_pins(0x00u);
  encoder_task();

  TEST_ASSERT_EQUAL_UINT8(1, process_count);
  TEST_ASSERT_EQUAL_UINT8(5, processed_keys[0]);
  TEST_ASSERT_TRUE(processed_pressed[0]);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_encoder_init_configures_phase_inputs);
  RUN_TEST(test_encoder_emits_clockwise_tap);
  RUN_TEST(test_encoder_queues_repeated_steps_until_previous_release);
  RUN_TEST(test_encoder_emits_counterclockwise_tap);
  return UNITY_END();
}
