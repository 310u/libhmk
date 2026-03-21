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
#include "analog_scan.h"

// GPIO ports for each ADC channel
static GPIO_TypeDef *channel_ports[] = {
    GPIOA, GPIOA, GPIOA, GPIOA, GPIOA, GPIOA, GPIOA, GPIOA,
    GPIOB, GPIOB, GPIOC, GPIOC, GPIOC, GPIOC, GPIOC, GPIOC,
};

_Static_assert(M_ARRAY_SIZE(channel_ports) == ADC_NUM_CHANNELS,
               "Invalid number of ADC channels");

// GPIO pins for each ADC channel
static const uint16_t channel_pins[] = {
    GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_2, GPIO_PIN_3, GPIO_PIN_4, GPIO_PIN_5,
    GPIO_PIN_6, GPIO_PIN_7, GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_0, GPIO_PIN_1,
    GPIO_PIN_2, GPIO_PIN_3, GPIO_PIN_4, GPIO_PIN_5,
};

_Static_assert(M_ARRAY_SIZE(channel_pins) == ADC_NUM_CHANNELS,
               "Invalid number of ADC channels");

#if ADC_NUM_MUX_INPUTS > 0
// ADC channels connected to each multiplexer input
static const uint8_t mux_input_channels[] = ADC_MUX_INPUT_CHANNELS;

_Static_assert(M_ARRAY_SIZE(mux_input_channels) == ADC_NUM_MUX_INPUTS,
               "Invalid number of ADC multiplexer inputs");

// GPIO ports for each multiplexer select pin
static GPIO_TypeDef *mux_select_ports[] = ADC_MUX_SELECT_PORTS;

_Static_assert(M_ARRAY_SIZE(mux_select_ports) == ADC_NUM_MUX_SELECT_PINS,
               "Invalid number of multiplexer select pins");

// GPIO pins for each multiplexer select pin
static const uint16_t mux_select_pins[] = ADC_MUX_SELECT_PINS;

_Static_assert(M_ARRAY_SIZE(mux_select_pins) == ADC_NUM_MUX_SELECT_PINS,
               "Invalid number of multiplexer select pins");
#endif

#if ADC_NUM_RAW_INPUTS > 0
// ADC channels connected to each raw input
static const uint8_t raw_input_channels[] = ADC_RAW_INPUT_CHANNELS;

_Static_assert(M_ARRAY_SIZE(raw_input_channels) == ADC_NUM_RAW_INPUTS,
               "Invalid number of ADC raw inputs");
#endif

#if DIGITAL_NUM_INPUTS > 0
// GPIO ports for each digital input.
static GPIO_TypeDef *digital_input_ports[] = DIGITAL_INPUT_PORTS;

_Static_assert(M_ARRAY_SIZE(digital_input_ports) == DIGITAL_NUM_INPUTS,
               "Invalid number of digital input ports");

// GPIO pins for each digital input.
static const uint16_t digital_input_pins[] = DIGITAL_INPUT_PINS;

_Static_assert(M_ARRAY_SIZE(digital_input_pins) == DIGITAL_NUM_INPUTS,
               "Invalid number of digital input pins");

// Vector containing the physical key number for each digital input.
static const uint16_t digital_input_vector[] = DIGITAL_INPUT_VECTOR;

_Static_assert(M_ARRAY_SIZE(digital_input_vector) == DIGITAL_NUM_INPUTS,
               "Invalid number of digital input mappings");

static void analog_enable_gpio_clock(GPIO_TypeDef *port) {
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

static void analog_init_digital_inputs(void) {
  for (uint32_t i = 0; i < DIGITAL_NUM_INPUTS; i++) {
    GPIO_InitTypeDef gpio_init = {0};

    analog_enable_gpio_clock(digital_input_ports[i]);

    gpio_init.Pin = digital_input_pins[i];
    gpio_init.Mode = GPIO_MODE_INPUT;
#if defined(DIGITAL_INPUT_PULLUP)
    gpio_init.Pull = GPIO_PULLUP;
#elif defined(DIGITAL_INPUT_PULLDOWN)
    gpio_init.Pull = GPIO_PULLDOWN;
#else
    gpio_init.Pull = GPIO_NOPULL;
#endif
    gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(digital_input_ports[i], &gpio_init);
  }
}

static bool analog_digital_input_pressed(uint32_t index) {
  const GPIO_PinState pin_state =
      HAL_GPIO_ReadPin(digital_input_ports[index], digital_input_pins[index]);

#if defined(DIGITAL_INPUT_ACTIVE_HIGH)
  return pin_state == GPIO_PIN_SET;
#else
  return pin_state == GPIO_PIN_RESET;
#endif
}

static bool analog_read_digital_input(uint8_t key, uint16_t *value) {
  const uint16_t physical_key = (uint16_t)key + 1u;

  for (uint32_t i = 0; i < DIGITAL_NUM_INPUTS; i++) {
    if (digital_input_vector[i] != physical_key) {
      continue;
    }

    *value = analog_digital_input_pressed(i) ? ADC_MAX_VALUE : 0u;
    return true;
  }

  return false;
}
#endif

static ADC_HandleTypeDef adc_handle;
static DMA_HandleTypeDef dma_handle;
#if ADC_NUM_MUX_INPUTS > 0
// We only need a timer to delay the multiplexer outputs
static TIM_HandleTypeDef tim_handle;
#endif

// Set to true when `adc_values` is filled for the first time
static volatile bool adc_initialized = false;
// Buffer for DMA transfer
__attribute__((aligned(8))) static volatile uint16_t
    adc_buffer[ADC_NUM_MUX_INPUTS + ADC_NUM_RAW_INPUTS];
void analog_init(void) {
  ADC_ChannelConfTypeDef channel_config = {0};

  // Enable peripheral clocks
  __HAL_RCC_ADC1_CLK_ENABLE();
  __HAL_RCC_DMA2_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
#if ADC_NUM_MUX_INPUTS > 0
  __HAL_RCC_TIM10_CLK_ENABLE();
#endif

#if DIGITAL_NUM_INPUTS > 0
  analog_init_digital_inputs();
#endif
  analog_scan_reset();

  // Initialize the ADC peripheral
  adc_handle.Instance = ADC1;
  adc_handle.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  adc_handle.Init.Resolution = ADC_RESOLUTION_HAL;
  adc_handle.Init.ScanConvMode = ENABLE;
  adc_handle.Init.ContinuousConvMode = DISABLE;
  adc_handle.Init.DiscontinuousConvMode = DISABLE;
  adc_handle.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  adc_handle.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  adc_handle.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  adc_handle.Init.NbrOfConversion = ADC_NUM_MUX_INPUTS + ADC_NUM_RAW_INPUTS;
  adc_handle.Init.DMAContinuousRequests = DISABLE;
  adc_handle.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&adc_handle) != HAL_OK)
    board_error_handler();

#if ADC_NUM_MUX_INPUTS > 0
  // Initialize the multiplexer input channels
  for (uint32_t i = 0; i < ADC_NUM_MUX_INPUTS; i++) {
    GPIO_InitTypeDef gpio_init = {0};

    channel_config.Channel = mux_input_channels[i];
    channel_config.Rank = i + 1;
    channel_config.SamplingTime = ADC_NUM_SAMPLE_CYCLES;
    if (HAL_ADC_ConfigChannel(&adc_handle, &channel_config) != HAL_OK)
      board_error_handler();

    gpio_init.Pin = channel_pins[mux_input_channels[i]];
    gpio_init.Mode = GPIO_MODE_ANALOG;
    gpio_init.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(channel_ports[mux_input_channels[i]], &gpio_init);
  }

  // Initialize multiplexer select pins
  for (uint32_t i = 0; i < ADC_NUM_MUX_SELECT_PINS; i++) {
    GPIO_InitTypeDef gpio_init = {0};

    gpio_init.Pin = mux_select_pins[i];
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(mux_select_ports[i], &gpio_init);

    HAL_GPIO_WritePin(mux_select_ports[i], mux_select_pins[i], GPIO_PIN_RESET);
  }
#endif

#if ADC_NUM_RAW_INPUTS > 0
  // Initialize the raw input channels
  for (uint32_t i = 0; i < ADC_NUM_RAW_INPUTS; i++) {
    GPIO_InitTypeDef gpio_init = {0};

    channel_config.Channel = raw_input_channels[i];
    channel_config.Rank = ADC_NUM_MUX_INPUTS + i + 1;
    channel_config.SamplingTime = ADC_NUM_SAMPLE_CYCLES;
    if (HAL_ADC_ConfigChannel(&adc_handle, &channel_config) != HAL_OK)
      board_error_handler();

    gpio_init.Pin = channel_pins[raw_input_channels[i]];
    gpio_init.Mode = GPIO_MODE_ANALOG;
    gpio_init.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(channel_ports[raw_input_channels[i]], &gpio_init);
  }
#endif

  // Initialize the DMA peripheral
  dma_handle.Instance = DMA2_Stream0;
  dma_handle.Init.Channel = DMA_CHANNEL_0;
  dma_handle.Init.Direction = DMA_PERIPH_TO_MEMORY;
  dma_handle.Init.PeriphInc = DMA_PINC_DISABLE;
  dma_handle.Init.MemInc = DMA_MINC_ENABLE;
  dma_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  dma_handle.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
  dma_handle.Init.Mode = DMA_CIRCULAR;
  dma_handle.Init.Priority = DMA_PRIORITY_HIGH;
  dma_handle.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
  if (HAL_DMA_Init(&dma_handle) != HAL_OK)
    board_error_handler();

  __HAL_LINKDMA(&adc_handle, DMA_Handle, dma_handle);

#if ADC_NUM_MUX_INPUTS > 0
  // Initialize the timer peripheral
  tim_handle.Instance = TIM10;
  tim_handle.Init.Prescaler = 0;
  tim_handle.Init.CounterMode = TIM_COUNTERMODE_UP;
  tim_handle.Init.Period = (F_CPU / 1000000) * ADC_SAMPLE_DELAY - 1;
  tim_handle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  tim_handle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&tim_handle) != HAL_OK)
    board_error_handler();
#endif

  // Enable interrupts
  HAL_NVIC_EnableIRQ(ADC_IRQn);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
#if ADC_NUM_MUX_INPUTS > 0
  HAL_NVIC_EnableIRQ(TIM1_UP_TIM10_IRQn);
#endif

  // Start the conversion loop
  HAL_ADC_Start_DMA(&adc_handle, (uint32_t *)adc_buffer,
                    ADC_NUM_MUX_INPUTS + ADC_NUM_RAW_INPUTS);

  // Wait for the ADC values to be initialized
  while (!adc_initialized)
    ;
}

void analog_task(void) {}

uint16_t analog_read(uint8_t key) {
#if defined(JOYSTICK_SW_KEY_INDEX) && defined(JOYSTICK_SW_PIN) &&                \
    defined(JOYSTICK_SW_PORT)
  if (key == JOYSTICK_SW_KEY_INDEX) {
    return HAL_GPIO_ReadPin(JOYSTICK_SW_PORT, JOYSTICK_SW_PIN) == GPIO_PIN_RESET
               ? ADC_MAX_VALUE
               : 0;
  }
#endif

#if DIGITAL_NUM_INPUTS > 0
  uint16_t digital_value = 0;
  if (analog_read_digital_input(key, &digital_value)) {
    return digital_value;
  }
#endif

  return analog_scan_read_key(key);
}

#if ADC_NUM_RAW_INPUTS > 0
uint16_t analog_read_raw(uint8_t index) { return analog_scan_read_raw(index); }
#endif

//--------------------------------------------------------------------+
// Interrupt Handlers
//--------------------------------------------------------------------+

void ADC_IRQHandler(void) { HAL_ADC_IRQHandler(&adc_handle); }

void DMA2_Stream0_IRQHandler(void) { HAL_DMA_IRQHandler(&dma_handle); }

#if ADC_NUM_MUX_INPUTS > 0
void TIM1_UP_TIM10_IRQHandler(void) { HAL_TIM_IRQHandler(&tim_handle); }
#endif

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
#if ADC_NUM_MUX_INPUTS > 0
  static uint8_t current_mux_channel = 0;
#endif

  if (hadc == &adc_handle) {
    uint8_t mux_channel = 0;
#if ADC_NUM_MUX_INPUTS > 0
    mux_channel = current_mux_channel;
#endif
    analog_scan_store_samples(adc_buffer, mux_channel);

#if ADC_NUM_MUX_INPUTS > 0
    current_mux_channel =
        (current_mux_channel + 1) & ((1 << ADC_NUM_MUX_SELECT_PINS) - 1);
    // We initialize all the ADC values when we have gone through all the
    // multiplexer input channels.
    adc_initialized |= (current_mux_channel == 0);

    // Set the multiplexer select pins
    for (uint32_t i = 0; i < ADC_NUM_MUX_SELECT_PINS; i++)
      HAL_GPIO_WritePin(mux_select_ports[i], mux_select_pins[i],
                        (current_mux_channel >> i) & 1);

    // Delay to allow the multiplexer outputs to settle
    HAL_TIM_Base_Start_IT(&tim_handle);
#else
    // We initialize all the ADC values when we have read all the raw input.
    adc_initialized = true;
    // Immediately start the next conversion
    HAL_ADC_Start_DMA(&adc_handle, (uint32_t *)adc_buffer,
                      ADC_NUM_MUX_INPUTS + ADC_NUM_RAW_INPUTS);
#endif
  }
}

#if ADC_NUM_MUX_INPUTS > 0
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  if (htim == &tim_handle) {
    // Stop the timer to prevent this callback from being called again while the
    // ADC is still converting
    HAL_TIM_Base_Stop_IT(&tim_handle);
    // Start the next conversion
    HAL_ADC_Start_DMA(&adc_handle, (uint32_t *)adc_buffer,
                      ADC_NUM_MUX_INPUTS + ADC_NUM_RAW_INPUTS);
  }
}
#endif
