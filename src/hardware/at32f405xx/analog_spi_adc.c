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

#include "analog_scan.h"
#include "at32f402_405.h"

#if defined(ANALOG_BACKEND_SPI_ADC)

#if !defined(SPI_ADC_DRIVER_ADS7953)
#error "Unsupported SPI ADC driver"
#endif

#if SPI_NUM_BUSES == 0
#error "spi_adc backend requires at least one configured SPI bus"
#endif

#if SPI_ADC_NUM_BUSES > 2
#error "spi_adc backend currently supports at most 2 active SPI buses"
#endif

#define ADS7953_CHANNELS_PER_DEVICE 16u
#define ADS7953_FRAMES_PER_SWEEP (ADS7953_CHANNELS_PER_DEVICE + 2u)
#define ADS7953_COMMAND_MANUAL_MODE 0x1800u
#define ADS7953_COMMAND_CHANNEL_SHIFT 7u
#define ADS7953_RESULT_CHANNEL_SHIFT 12u
#define ADS7953_RESULT_CHANNEL_MASK 0x0Fu

#if defined(SPI_ADC_RANGE_2X_VREF)
#define ADS7953_COMMAND_RANGE_BIT 0x0040u
#else
#define ADS7953_COMMAND_RANGE_BIT 0x0000u
#endif

_Static_assert(ADC_NUM_MUX_INPUTS == 0,
               "spi_adc backend expects mux inputs to be flattened at build time");
_Static_assert(ADC_NUM_RAW_INPUTS ==
                   (SPI_ADC_NUM_DEVICES * ADS7953_CHANNELS_PER_DEVICE),
               "spi_adc backend expects 16 raw inputs per ADS7953 device");

static const uint8_t spi_adc_bus_ids[] = SPI_ADC_BUS_IDS;
static const uint8_t spi_adc_device_bus[] = SPI_ADC_DEVICE_BUS;
static gpio_type *spi_adc_device_cs_ports[] = SPI_ADC_DEVICE_CS_PORTS;
static const uint16_t spi_adc_device_cs_pins[] = SPI_ADC_DEVICE_CS_PINS;

_Static_assert(M_ARRAY_SIZE(spi_adc_bus_ids) == SPI_ADC_NUM_BUSES,
               "Invalid number of SPI ADC buses");
_Static_assert(M_ARRAY_SIZE(spi_adc_device_bus) == SPI_ADC_NUM_DEVICES,
               "Invalid number of SPI ADC device bus mappings");
_Static_assert(M_ARRAY_SIZE(spi_adc_device_cs_ports) == SPI_ADC_NUM_DEVICES,
               "Invalid number of SPI ADC chip select ports");
_Static_assert(M_ARRAY_SIZE(spi_adc_device_cs_pins) == SPI_ADC_NUM_DEVICES,
               "Invalid number of SPI ADC chip select pins");

typedef struct {
  spi_type *instance;
  uint32_t clock_hz;
} spi_adc_hw_bus_t;

static const spi_adc_hw_bus_t spi_adc_hw_buses[] = {
#if SPI_NUM_BUSES > 0
    {
        .instance = SPI_BUS0_INSTANCE,
        .clock_hz = SPI_BUS0_CLOCK_HZ,
    },
#endif
#if SPI_NUM_BUSES > 1
    {
        .instance = SPI_BUS1_INSTANCE,
        .clock_hz = SPI_BUS1_CLOCK_HZ,
    },
#endif
#if SPI_NUM_BUSES > 2
    {
        .instance = SPI_BUS2_INSTANCE,
        .clock_hz = SPI_BUS2_CLOCK_HZ,
    },
#endif
#if SPI_NUM_BUSES > 3
    {
        .instance = SPI_BUS3_INSTANCE,
        .clock_hz = SPI_BUS3_CLOCK_HZ,
    },
#endif
};

_Static_assert(M_ARRAY_SIZE(spi_adc_hw_buses) == SPI_NUM_BUSES,
               "SPI bus macro definitions are incomplete");

typedef struct {
  uint8_t bus_id;
  spi_type *instance;
  uint32_t clock_hz;
  dma_channel_type *rx_dma;
  dma_channel_type *tx_dma;
  dmamux_channel_type *rx_mux;
  dmamux_channel_type *tx_mux;
  dmamux_requst_id_sel_type rx_request;
  dmamux_requst_id_sel_type tx_request;
  uint32_t rx_flag;
  IRQn_Type rx_irq;
  uint8_t device_indices[2];
  uint8_t num_devices;
  uint8_t current_device_slot;
} spi_adc_bus_state_t;

static spi_chip_select_t spi_adc_chip_selects[SPI_ADC_NUM_DEVICES];
static spi_adc_bus_state_t spi_adc_buses[SPI_ADC_NUM_BUSES];

__attribute__((aligned(8))) static uint16_t
    spi_adc_tx_frames[SPI_ADC_NUM_DEVICES][ADS7953_FRAMES_PER_SWEEP];
__attribute__((aligned(8))) static volatile uint16_t
    spi_adc_rx_frames[SPI_ADC_NUM_DEVICES][ADS7953_FRAMES_PER_SWEEP];
static uint16_t spi_adc_scan_buffer[ADC_NUM_RAW_INPUTS];

static volatile bool spi_adc_initialized = false;
static volatile uint8_t spi_adc_completed_buses = 0;

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

static spi_mclk_freq_div_type spi_adc_pick_divider(uint32_t clock_hz,
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

static dmamux_requst_id_sel_type spi_adc_dma_request(spi_type *instance,
                                                     bool rx) {
  if (instance == SPI1) {
    return rx ? DMAMUX_DMAREQ_ID_SPI1_RX : DMAMUX_DMAREQ_ID_SPI1_TX;
  }
  if (instance == SPI2) {
    return rx ? DMAMUX_DMAREQ_ID_SPI2_RX : DMAMUX_DMAREQ_ID_SPI2_TX;
  }
  if (instance == SPI3) {
    return rx ? DMAMUX_DMAREQ_ID_SPI3_RX : DMAMUX_DMAREQ_ID_SPI3_TX;
  }

  board_error_handler();
  return DMAMUX_DMAREQ_ID_SPI1_RX;
}

static void spi_adc_init_dma_channel(dma_channel_type *channel,
                                     uint32_t peripheral_base_addr,
                                     dma_dir_type direction) {
  dma_init_type dma_init_struct;

  dma_reset(channel);
  dma_default_para_init(&dma_init_struct);
  dma_init_struct.buffer_size = ADS7953_FRAMES_PER_SWEEP;
  dma_init_struct.direction = direction;
  dma_init_struct.memory_data_width = DMA_MEMORY_DATA_WIDTH_HALFWORD;
  dma_init_struct.memory_inc_enable = TRUE;
  dma_init_struct.peripheral_base_addr = peripheral_base_addr;
  dma_init_struct.peripheral_data_width = DMA_PERIPHERAL_DATA_WIDTH_HALFWORD;
  dma_init_struct.peripheral_inc_enable = FALSE;
  dma_init_struct.priority = DMA_PRIORITY_VERY_HIGH;
  dma_init_struct.loop_mode_enable = FALSE;
  dma_init(channel, &dma_init_struct);
}

static void spi_adc_configure_instance(const spi_adc_bus_state_t *bus) {
  spi_init_type spi_init_struct;

  spi_default_para_init(&spi_init_struct);
  spi_init_struct.transmission_mode = SPI_TRANSMIT_FULL_DUPLEX;
  spi_init_struct.master_slave_mode = SPI_MODE_MASTER;
  spi_init_struct.mclk_freq_division =
      spi_adc_pick_divider(bus->clock_hz, SPI_ADC_FREQUENCY_HZ);
  spi_init_struct.first_bit_transmission = SPI_FIRST_BIT_MSB;
  spi_init_struct.frame_bit_num = SPI_FRAME_16BIT;
  spi_init_struct.clock_polarity = SPI_CLOCK_POLARITY_LOW;
  spi_init_struct.clock_phase = SPI_CLOCK_PHASE_1EDGE;
  spi_init_struct.cs_mode_selection = SPI_CS_SOFTWARE_MODE;

  spi_enable(bus->instance, FALSE);
  spi_init(bus->instance, &spi_init_struct);
  spi_software_cs_internal_level_set(bus->instance,
                                     SPI_SWCS_INTERNAL_LEVEL_HIGHT);
  spi_i2s_dma_transmitter_enable(bus->instance, FALSE);
  spi_i2s_dma_receiver_enable(bus->instance, FALSE);
  spi_enable(bus->instance, TRUE);
}

static void spi_adc_prepare_tx_frames(void) {
  const uint16_t command_base =
      ADS7953_COMMAND_MANUAL_MODE | ADS7953_COMMAND_RANGE_BIT;

  for (uint32_t device = 0; device < SPI_ADC_NUM_DEVICES; device++) {
    for (uint32_t channel = 0; channel < ADS7953_CHANNELS_PER_DEVICE; channel++) {
      spi_adc_tx_frames[device][channel] =
          command_base | (uint16_t)(channel << ADS7953_COMMAND_CHANNEL_SHIFT);
    }

    // Two trailing frames flush the device's two-frame conversion pipeline.
    spi_adc_tx_frames[device][ADS7953_CHANNELS_PER_DEVICE] = command_base;
    spi_adc_tx_frames[device][ADS7953_CHANNELS_PER_DEVICE + 1u] = command_base;
  }
}

static void spi_adc_assign_bus_resources(spi_adc_bus_state_t *bus,
                                         uint8_t logical_bus_index) {
  switch (logical_bus_index) {
  case 0:
    bus->rx_dma = DMA2_CHANNEL1;
    bus->tx_dma = DMA2_CHANNEL2;
    bus->rx_mux = DMA2MUX_CHANNEL1;
    bus->tx_mux = DMA2MUX_CHANNEL2;
    bus->rx_flag = DMA2_FDT1_FLAG;
    bus->rx_irq = DMA2_Channel1_IRQn;
    return;

  case 1:
    bus->rx_dma = DMA2_CHANNEL3;
    bus->tx_dma = DMA2_CHANNEL4;
    bus->rx_mux = DMA2MUX_CHANNEL3;
    bus->tx_mux = DMA2MUX_CHANNEL4;
    bus->rx_flag = DMA2_FDT3_FLAG;
    bus->rx_irq = DMA2_Channel3_IRQn;
    return;

  default:
    board_error_handler();
    return;
  }
}

static void spi_adc_init_bus_state(spi_adc_bus_state_t *bus,
                                   uint8_t logical_bus_index) {
  uint8_t write_index = 0;

  memset(bus, 0, sizeof(*bus));
  bus->bus_id = spi_adc_bus_ids[logical_bus_index];
  if (bus->bus_id >= M_ARRAY_SIZE(spi_adc_hw_buses)) {
    board_error_handler();
  }

  bus->instance = spi_adc_hw_buses[bus->bus_id].instance;
  bus->clock_hz = spi_adc_hw_buses[bus->bus_id].clock_hz;
  bus->rx_request = spi_adc_dma_request(bus->instance, true);
  bus->tx_request = spi_adc_dma_request(bus->instance, false);

  for (uint8_t device = 0; device < SPI_ADC_NUM_DEVICES; device++) {
    if (spi_adc_device_bus[device] != bus->bus_id) {
      continue;
    }

    if (write_index >= M_ARRAY_SIZE(bus->device_indices)) {
      board_error_handler();
    }

    bus->device_indices[write_index++] = device;
  }

  if (write_index == 0u) {
    board_error_handler();
  }

  bus->num_devices = write_index;
  spi_adc_assign_bus_resources(bus, logical_bus_index);
}

static void spi_adc_start_bus_transfer(spi_adc_bus_state_t *bus,
                                       uint8_t device_slot) {
  const uint8_t device_index = bus->device_indices[device_slot];

  bus->current_device_slot = device_slot;

  dma_channel_enable(bus->rx_dma, FALSE);
  dma_channel_enable(bus->tx_dma, FALSE);
  dma_flag_clear(bus->rx_flag);

  bus->rx_dma->maddr = (uint32_t)spi_adc_rx_frames[device_index];
  bus->rx_dma->dtcnt = ADS7953_FRAMES_PER_SWEEP;
  bus->tx_dma->maddr = (uint32_t)spi_adc_tx_frames[device_index];
  bus->tx_dma->dtcnt = ADS7953_FRAMES_PER_SWEEP;

  spi_cs_select(&spi_adc_chip_selects[device_index]);
  dma_channel_enable(bus->rx_dma, TRUE);
  dma_channel_enable(bus->tx_dma, TRUE);
  spi_i2s_dma_receiver_enable(bus->instance, TRUE);
  spi_i2s_dma_transmitter_enable(bus->instance, TRUE);
}

static void spi_adc_start_scan_cycle(void) {
  spi_adc_completed_buses = 0;
  memset(spi_adc_scan_buffer, 0, sizeof(spi_adc_scan_buffer));

  for (uint32_t i = 0; i < SPI_ADC_NUM_BUSES; i++) {
    spi_adc_start_bus_transfer(&spi_adc_buses[i], 0);
  }
}

static void spi_adc_store_device_samples(uint8_t device_index) {
  const uint32_t raw_offset = (uint32_t)device_index * ADS7953_CHANNELS_PER_DEVICE;

  for (uint32_t frame = 0; frame < ADS7953_FRAMES_PER_SWEEP; frame++) {
    const uint16_t value = spi_adc_rx_frames[device_index][frame];
    const uint8_t channel = (uint8_t)((value >> ADS7953_RESULT_CHANNEL_SHIFT) &
                                      ADS7953_RESULT_CHANNEL_MASK);

    if (channel >= ADS7953_CHANNELS_PER_DEVICE) {
      continue;
    }

    spi_adc_scan_buffer[raw_offset + channel] = value & ADC_MAX_VALUE;
  }
}

static void spi_adc_complete_bus_transfer(spi_adc_bus_state_t *bus) {
  const uint8_t device_index = bus->device_indices[bus->current_device_slot];

  dma_channel_enable(bus->rx_dma, FALSE);
  dma_channel_enable(bus->tx_dma, FALSE);
  spi_i2s_dma_receiver_enable(bus->instance, FALSE);
  spi_i2s_dma_transmitter_enable(bus->instance, FALSE);
  while (spi_i2s_flag_get(bus->instance, SPI_I2S_BF_FLAG) != RESET) {
  }
  spi_cs_deselect(&spi_adc_chip_selects[device_index]);

  spi_adc_store_device_samples(device_index);

  if ((uint8_t)(bus->current_device_slot + 1u) < bus->num_devices) {
    spi_adc_start_bus_transfer(bus, (uint8_t)(bus->current_device_slot + 1u));
    return;
  }

  spi_adc_completed_buses++;
  if (spi_adc_completed_buses < SPI_ADC_NUM_BUSES) {
    return;
  }

  analog_scan_store_samples(spi_adc_scan_buffer, 0);
  spi_adc_initialized = true;
  spi_adc_start_scan_cycle();
}

static void spi_adc_init_buses(void) {
  crm_periph_clock_enable(CRM_DMA2_PERIPH_CLOCK, TRUE);
  dmamux_enable(DMA2, TRUE);
  spi_bus_init();
  spi_adc_prepare_tx_frames();

  for (uint32_t i = 0; i < SPI_ADC_NUM_DEVICES; i++) {
    spi_adc_chip_selects[i].port = spi_adc_device_cs_ports[i];
    spi_adc_chip_selects[i].pin = spi_adc_device_cs_pins[i];
    spi_adc_chip_selects[i].active_low = true;
    spi_cs_init(&spi_adc_chip_selects[i]);
  }

  for (uint32_t i = 0; i < SPI_ADC_NUM_BUSES; i++) {
    spi_adc_bus_state_t *bus = &spi_adc_buses[i];

    spi_adc_init_bus_state(bus, (uint8_t)i);
    spi_adc_configure_instance(bus);

    spi_adc_init_dma_channel(bus->rx_dma, (uint32_t)&bus->instance->dt,
                             DMA_DIR_PERIPHERAL_TO_MEMORY);
    spi_adc_init_dma_channel(bus->tx_dma, (uint32_t)&bus->instance->dt,
                             DMA_DIR_MEMORY_TO_PERIPHERAL);
    dmamux_init(bus->rx_mux, bus->rx_request);
    dmamux_init(bus->tx_mux, bus->tx_request);
    dma_interrupt_enable(bus->rx_dma, DMA_FDT_INT, TRUE);
    nvic_irq_enable(bus->rx_irq, 0, 0);
  }
}

void analog_init(void) {
#if DIGITAL_NUM_INPUTS > 0
  analog_init_digital_inputs();
#endif

  analog_scan_reset();
  spi_adc_init_buses();
  spi_adc_start_scan_cycle();

  while (!spi_adc_initialized) {
  }
}

void analog_task(void) {}

uint16_t analog_read(uint8_t key) {
#if defined(JOYSTICK_SW_KEY_INDEX) && defined(JOYSTICK_SW_PIN) &&               \
    defined(JOYSTICK_SW_PORT)
  if (key == JOYSTICK_SW_KEY_INDEX) {
    return gpio_input_data_bit_read(JOYSTICK_SW_PORT, JOYSTICK_SW_PIN) == RESET
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

static void spi_adc_handle_dma_irq(uint8_t logical_bus_index) {
  spi_adc_bus_state_t *bus;

  if (logical_bus_index >= SPI_ADC_NUM_BUSES) {
    return;
  }

  bus = &spi_adc_buses[logical_bus_index];
  if (dma_interrupt_flag_get(bus->rx_flag) != SET) {
    return;
  }

  dma_flag_clear(bus->rx_flag);
  spi_adc_complete_bus_transfer(bus);
}

void DMA2_Channel1_IRQHandler(void) { spi_adc_handle_dma_irq(0); }

#if SPI_ADC_NUM_BUSES > 1
void DMA2_Channel3_IRQHandler(void) { spi_adc_handle_dma_irq(1); }
#endif

#endif
