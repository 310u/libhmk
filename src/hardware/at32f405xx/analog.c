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
#include "analog_scan.h"

// GPIO ports for each ADC channel
static gpio_type *channel_ports[] = {
    GPIOA, GPIOA, GPIOA, GPIOA, GPIOA, GPIOA, GPIOA, GPIOA,
    GPIOB, GPIOB, GPIOC, GPIOC, GPIOC, GPIOC, GPIOC, GPIOC,
};

_Static_assert(M_ARRAY_SIZE(channel_ports) == ADC_NUM_CHANNELS,
               "Invalid number of ADC channels");

// GPIO pins for each ADC channel
static const uint16_t channel_pins[] = {
    GPIO_PINS_0, GPIO_PINS_1, GPIO_PINS_2, GPIO_PINS_3,
    GPIO_PINS_4, GPIO_PINS_5, GPIO_PINS_6, GPIO_PINS_7,
    GPIO_PINS_0, GPIO_PINS_1, GPIO_PINS_0, GPIO_PINS_1,
    GPIO_PINS_2, GPIO_PINS_3, GPIO_PINS_4, GPIO_PINS_5,
};

_Static_assert(M_ARRAY_SIZE(channel_pins) == ADC_NUM_CHANNELS,
               "Invalid number of ADC channels");

#if ADC_NUM_MUX_INPUTS > 0
// ADC channels connected to each multiplexer input
static const uint8_t mux_input_channels[] = ADC_MUX_INPUT_CHANNELS;

_Static_assert(M_ARRAY_SIZE(mux_input_channels) == ADC_NUM_MUX_INPUTS,
               "Invalid number of ADC multiplexer inputs");

// GPIO ports for each multiplexer select pin
static gpio_type *mux_select_ports[] = ADC_MUX_SELECT_PORTS;

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
static gpio_type *digital_input_ports[] = DIGITAL_INPUT_PORTS;

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

static void analog_enable_gpio_clock(gpio_type *port) {
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

static void analog_init_digital_inputs(void) {
  for (uint32_t i = 0; i < DIGITAL_NUM_INPUTS; i++) {
    gpio_init_type digital_gpio_init;

    analog_enable_gpio_clock(digital_input_ports[i]);

    gpio_default_para_init(&digital_gpio_init);
    digital_gpio_init.gpio_pins = digital_input_pins[i];
    digital_gpio_init.gpio_mode = GPIO_MODE_INPUT;
#if defined(DIGITAL_INPUT_PULLUP)
    digital_gpio_init.gpio_pull = GPIO_PULL_UP;
#elif defined(DIGITAL_INPUT_PULLDOWN)
    digital_gpio_init.gpio_pull = GPIO_PULL_DOWN;
#else
    digital_gpio_init.gpio_pull = GPIO_PULL_NONE;
#endif
    gpio_init(digital_input_ports[i], &digital_gpio_init);
  }
}

static bool analog_digital_input_pressed(uint32_t index) {
#if defined(DIGITAL_INPUT_ACTIVE_HIGH)
  return gpio_input_data_bit_read(digital_input_ports[index],
                                  digital_input_pins[index]) != RESET;
#else
  return gpio_input_data_bit_read(digital_input_ports[index],
                                  digital_input_pins[index]) == RESET;
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

static adc_base_config_type adc_base_struct;
static dma_init_type dma_init_struct;
static gpio_init_type gpio_init_struct;

// Set to true when `adc_values` is filled for the first time
static volatile bool adc_initialized = false;
// Buffer for DMA transfer
__attribute__((aligned(8))) static volatile uint16_t
    adc_buffer[ADC_NUM_MUX_INPUTS + ADC_NUM_RAW_INPUTS];
void analog_init(void) {
  // Enable peripheral clocks
  crm_periph_clock_enable(CRM_ADC1_PERIPH_CLOCK, TRUE);
  crm_periph_clock_enable(CRM_DMA1_PERIPH_CLOCK, TRUE);
  crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
  crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
  crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
#if ADC_NUM_MUX_INPUTS > 0
  crm_periph_clock_enable(CRM_TMR6_PERIPH_CLOCK, TRUE);
#endif

#if DIGITAL_NUM_INPUTS > 0
  analog_init_digital_inputs();
#endif
  analog_scan_reset();

  // Initialize the ADC peripheral
  adc_clock_div_set(ADC_DIV_8);
  adc_reset(ADC1);
  adc_base_default_para_init(&adc_base_struct);
  adc_base_struct.sequence_mode = TRUE;
  adc_base_struct.repeat_mode = FALSE;
  adc_base_struct.data_align = ADC_RIGHT_ALIGNMENT;
  adc_base_struct.ordinary_channel_length =
      ADC_NUM_MUX_INPUTS + ADC_NUM_RAW_INPUTS;
  adc_base_config(ADC1, &adc_base_struct);

#if ADC_NUM_MUX_INPUTS > 0
  // Initialize the multiplexer input channels
  for (uint32_t i = 0; i < ADC_NUM_MUX_INPUTS; i++) {
    adc_ordinary_channel_set(ADC1,
                             (adc_channel_select_type)mux_input_channels[i],
                             i + 1, ADC_NUM_SAMPLE_CYCLES);

    gpio_default_para_init(&gpio_init_struct);
    gpio_init_struct.gpio_mode = GPIO_MODE_ANALOG;
    gpio_init_struct.gpio_pins = channel_pins[mux_input_channels[i]];
    gpio_init(channel_ports[mux_input_channels[i]], &gpio_init_struct);
  }

  // Initialize multiplexer select pins
  for (uint32_t i = 0; i < ADC_NUM_MUX_SELECT_PINS; i++) {
    gpio_default_para_init(&gpio_init_struct);
    gpio_init_struct.gpio_pins = mux_select_pins[i];
    gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
    gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init(mux_select_ports[i], &gpio_init_struct);

    gpio_bits_write(mux_select_ports[i], mux_select_pins[i], TRUE);
  }
#endif

#if ADC_NUM_RAW_INPUTS > 0
  // Initialize the raw input channels
  for (uint32_t i = 0; i < ADC_NUM_RAW_INPUTS; i++) {
    adc_ordinary_channel_set(ADC1,
                             (adc_channel_select_type)raw_input_channels[i],
                             ADC_NUM_MUX_INPUTS + i + 1, ADC_NUM_SAMPLE_CYCLES);

    gpio_default_para_init(&gpio_init_struct);
    gpio_init_struct.gpio_pins = channel_pins[raw_input_channels[i]];
    gpio_init_struct.gpio_mode = GPIO_MODE_ANALOG;
    gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
    gpio_init(channel_ports[raw_input_channels[i]], &gpio_init_struct);
  }
#endif

  // Configure the ADC trigger source as software
  adc_ordinary_conversion_trigger_set(ADC1, ADC12_ORDINARY_TRIG_SOFTWARE, TRUE);

  // Initialize the DMA peripheral
  dma_reset(DMA1_CHANNEL1);
  dma_default_para_init(&dma_init_struct);
  dma_init_struct.buffer_size = ADC_NUM_MUX_INPUTS + ADC_NUM_RAW_INPUTS;
  dma_init_struct.direction = DMA_DIR_PERIPHERAL_TO_MEMORY;
  dma_init_struct.memory_base_addr = (uint32_t)adc_buffer;
  dma_init_struct.memory_data_width = DMA_MEMORY_DATA_WIDTH_HALFWORD;
  dma_init_struct.memory_inc_enable = TRUE;
  dma_init_struct.peripheral_base_addr = (uint32_t)&ADC1->odt;
  dma_init_struct.peripheral_data_width = DMA_PERIPHERAL_DATA_WIDTH_HALFWORD;
  dma_init_struct.peripheral_inc_enable = FALSE;
  dma_init_struct.priority = DMA_PRIORITY_HIGH;
  dma_init_struct.loop_mode_enable = TRUE;
  dma_init(DMA1_CHANNEL1, &dma_init_struct);

  // Configure the DMA multiplexer to use ADC1
  dmamux_enable(DMA1, TRUE);
  dmamux_init(DMA1MUX_CHANNEL1, DMAMUX_DMAREQ_ID_ADC1);
  // Enable DMA transfer complete interrupt
  dma_interrupt_enable(DMA1_CHANNEL1, DMA_FDT_INT, TRUE);

  // Enable ADC DMA mode
  adc_dma_mode_enable(ADC1, TRUE);

#if ADC_NUM_MUX_INPUTS > 0
  // Initialize the timer peripheral
  tmr_base_init(TMR6, (F_CPU / 1000000) * ADC_SAMPLE_DELAY - 1, 0);
  tmr_cnt_dir_set(TMR6, TMR_COUNT_UP);
  tmr_interrupt_enable(TMR6, TMR_OVF_INT, TRUE);
#endif

  // Enable interrupts
  nvic_irq_enable(ADC1_IRQn, 0, 0);
  nvic_irq_enable(DMA1_Channel1_IRQn, 0, 0);
#if ADC_NUM_MUX_INPUTS > 0
  nvic_irq_enable(TMR6_GLOBAL_IRQn, 0, 0);
#endif

  // Enable the ADC peripheral
  adc_enable(ADC1, TRUE);

  // Calibrate the ADC
  adc_calibration_init(ADC1);
  while (adc_calibration_init_status_get(ADC1) == SET)
    ;
  adc_calibration_start(ADC1);
  while (adc_calibration_status_get(ADC1) == SET)
    ;

  // Enable DMA after ADC initialization
  dma_channel_enable(DMA1_CHANNEL1, TRUE);
  // Start the ADC conversion
  adc_ordinary_software_trigger_enable(ADC1, TRUE);

  // Wait for the ADC values to be initialized
  while (!adc_initialized)
    ;
}

void analog_task(void) {}

uint16_t analog_read(uint8_t key) { 
#if defined(JOYSTICK_SW_KEY_INDEX) && defined(JOYSTICK_SW_PIN) && defined(JOYSTICK_SW_PORT)
  if (key == JOYSTICK_SW_KEY_INDEX) {
    return gpio_input_data_bit_read(JOYSTICK_SW_PORT, JOYSTICK_SW_PIN) == RESET ? ADC_MAX_VALUE : 0;
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

void DMA1_Channel1_IRQHandler(void) {
#if ADC_NUM_MUX_INPUTS > 0
  static uint8_t current_mux_channel = 0;
#endif

  if (dma_interrupt_flag_get(DMA1_FDT1_FLAG) == SET) {
    // Clear the DMA transfer complete flag
    dma_flag_clear(DMA1_FDT1_FLAG);

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
      gpio_bits_write(mux_select_ports[i], mux_select_pins[i],
                      (confirm_state)((current_mux_channel >> i) & 1));

    // Delay to allow the multiplexer outputs to settle
    tmr_counter_enable(TMR6, TRUE);
#else
    // We initialize all the ADC values when we have read all the raw input.
    adc_initialized = true;
    // Immediately start the next conversion
    adc_ordinary_software_trigger_enable(ADC1, TRUE);
#endif
  }
}

#if ADC_NUM_MUX_INPUTS > 0
void TMR6_GLOBAL_IRQHandler(void) {
  if (tmr_interrupt_flag_get(TMR6, TMR_OVF_INT) == SET) {
    // Clear the timer overflow flag
    tmr_flag_clear(TMR6, TMR_OVF_INT);
    // Disable the timer to stop the delay
    tmr_counter_enable(TMR6, FALSE);
    // Start the next conversion
    adc_ordinary_software_trigger_enable(ADC1, TRUE);
  }
}
#endif
