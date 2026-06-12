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
#include "usb_bootstrap.h"

#if defined(ANALOG_BACKEND_SPI_ADC)

#if !defined(SPI_ADC_DRIVER_ADS7953)
#error "Unsupported SPI ADC driver"
#endif

#if SPI_ADC_NUM_BUSES > 2
#error "spi_adc backend currently supports at most 2 active SPI buses"
#endif

#define ADS7953_CHANNELS_PER_DEVICE 16u
#define ADS7953_PIPELINE_FLUSH_FRAMES 2u
#define ADS7953_MAX_FRAMES_PER_DEVICE                                         \
  (ADS7953_CHANNELS_PER_DEVICE + ADS7953_PIPELINE_FLUSH_FRAMES)
#define ADS7953_COMMAND_MANUAL_MODE 0x1800u
#define ADS7953_COMMAND_MANUAL_SCAN 0x1000u
#define ADS7953_COMMAND_WRITE_BIT 0x0800u
#define ADS7953_COMMAND_CHANNEL_SHIFT 7u
#define ADS7953_RESULT_CHANNEL_SHIFT 12u
#define ADS7953_RESULT_CHANNEL_MASK 0x0Fu
#define SPI_ADC_TIMING_MODE_COUNT 4u
#define SPI_ADC_MODE_PROBE_SCAN_COUNT 4u

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
static const uint8_t spi_adc_device_scan_counts[] = SPI_ADC_DEVICE_SCAN_COUNTS;
static const uint8_t spi_adc_device_scan_channels[][ADS7953_CHANNELS_PER_DEVICE] =
    SPI_ADC_DEVICE_SCAN_CHANNELS;
static gpio_type *spi_adc_device_cs_ports[] = SPI_ADC_DEVICE_CS_PORTS;
static const uint16_t spi_adc_device_cs_pins[] = SPI_ADC_DEVICE_CS_PINS;

_Static_assert(M_ARRAY_SIZE(spi_adc_bus_ids) == SPI_ADC_NUM_BUSES,
               "Invalid number of SPI ADC buses");
_Static_assert(M_ARRAY_SIZE(spi_adc_device_bus) == SPI_ADC_NUM_DEVICES,
               "Invalid number of SPI ADC device bus mappings");
_Static_assert(M_ARRAY_SIZE(spi_adc_device_scan_counts) == SPI_ADC_NUM_DEVICES,
               "Invalid number of SPI ADC scan counts");
_Static_assert(M_ARRAY_SIZE(spi_adc_device_scan_channels) ==
                   SPI_ADC_NUM_DEVICES,
               "Invalid number of SPI ADC scan-channel maps");
_Static_assert(M_ARRAY_SIZE(spi_adc_device_cs_ports) == SPI_ADC_NUM_DEVICES,
               "Invalid number of SPI ADC chip select ports");
_Static_assert(M_ARRAY_SIZE(spi_adc_device_cs_pins) == SPI_ADC_NUM_DEVICES,
               "Invalid number of SPI ADC chip select pins");

typedef struct {
  gpio_type *sck_port;
  uint16_t sck_pin;
  gpio_type *miso_port;
  uint16_t miso_pin;
  gpio_type *mosi_port;
  uint16_t mosi_pin;
} spi_adc_gpio_bus_t;

static const spi_adc_gpio_bus_t spi_adc_gpio_buses[] = {
#if defined(SPI_ADC_BUS0_SCK_PORT) && defined(SPI_ADC_BUS0_SCK_PIN) &&         \
    defined(SPI_ADC_BUS0_MISO_PORT) && defined(SPI_ADC_BUS0_MISO_PIN) &&       \
    defined(SPI_ADC_BUS0_MOSI_PORT) && defined(SPI_ADC_BUS0_MOSI_PIN)
    {
        .sck_port = SPI_ADC_BUS0_SCK_PORT,
        .sck_pin = SPI_ADC_BUS0_SCK_PIN,
        .miso_port = SPI_ADC_BUS0_MISO_PORT,
        .miso_pin = SPI_ADC_BUS0_MISO_PIN,
        .mosi_port = SPI_ADC_BUS0_MOSI_PORT,
        .mosi_pin = SPI_ADC_BUS0_MOSI_PIN,
    },
#endif
#if defined(SPI_ADC_BUS1_SCK_PORT) && defined(SPI_ADC_BUS1_SCK_PIN) &&         \
    defined(SPI_ADC_BUS1_MISO_PORT) && defined(SPI_ADC_BUS1_MISO_PIN) &&       \
    defined(SPI_ADC_BUS1_MOSI_PORT) && defined(SPI_ADC_BUS1_MOSI_PIN)
    {
        .sck_port = SPI_ADC_BUS1_SCK_PORT,
        .sck_pin = SPI_ADC_BUS1_SCK_PIN,
        .miso_port = SPI_ADC_BUS1_MISO_PORT,
        .miso_pin = SPI_ADC_BUS1_MISO_PIN,
        .mosi_port = SPI_ADC_BUS1_MOSI_PORT,
        .mosi_pin = SPI_ADC_BUS1_MOSI_PIN,
    },
#endif
};

_Static_assert(M_ARRAY_SIZE(spi_adc_gpio_buses) > 0,
               "spi_adc backend requires SPI_ADC_BUSn GPIO definitions");

typedef struct {
  uint8_t logical_bus_index;
  uint8_t bus_id;
  uint8_t device_indices[2];
  uint8_t num_devices;
  uint8_t timing_mode;
} spi_adc_bus_state_t;

static spi_chip_select_t spi_adc_chip_selects[SPI_ADC_NUM_DEVICES];
static spi_adc_bus_state_t spi_adc_buses[SPI_ADC_NUM_BUSES];
static uint16_t spi_adc_scan_buffer[ADC_NUM_RAW_INPUTS];
static uint16_t
    spi_adc_last_rx_words[SPI_ADC_NUM_DEVICES][ADS7953_MAX_FRAMES_PER_DEVICE];
static uint16_t spi_adc_device_channel_masks[SPI_ADC_NUM_DEVICES];
static uint32_t spi_adc_half_period_cycles;
static analog_scan_diagnostics_t analog_scan_diagnostics;

#if DIGITAL_NUM_INPUTS > 0
static gpio_type *digital_input_ports[] = DIGITAL_INPUT_PORTS;
static const uint16_t digital_input_pins[] = DIGITAL_INPUT_PINS;
static const uint16_t digital_input_vector[] = DIGITAL_INPUT_VECTOR;

_Static_assert(M_ARRAY_SIZE(digital_input_ports) == DIGITAL_NUM_INPUTS,
               "Invalid number of digital input ports");
_Static_assert(M_ARRAY_SIZE(digital_input_pins) == DIGITAL_NUM_INPUTS,
               "Invalid number of digital input pins");
_Static_assert(M_ARRAY_SIZE(digital_input_vector) == DIGITAL_NUM_INPUTS,
               "Invalid number of digital input mappings");
#endif

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

#if DIGITAL_NUM_INPUTS > 0
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

static void spi_adc_delay_cycles(uint32_t cycles) {
  if (cycles == 0u) {
    return;
  }

  const uint32_t start = board_cycle_count();
  while ((uint32_t)(board_cycle_count() - start) < cycles) {
  }
}

static uint32_t spi_adc_pick_half_period_cycles(void) {
#if defined(F_CPU) && F_CPU > 0
  if (SPI_ADC_FREQUENCY_HZ == 0u) {
    return 0u;
  }

  return (uint32_t)((uint64_t)F_CPU /
                    ((uint64_t)SPI_ADC_FREQUENCY_HZ * 2ull));
#else
  return 0u;
#endif
}

static uint32_t spi_adc_cycles_to_us(uint32_t cycles) {
#if defined(F_CPU) && F_CPU > 0
  return (uint32_t)(((uint64_t)cycles * 1000000ull) / (uint64_t)F_CPU);
#else
  (void)cycles;
  return 0u;
#endif
}

static uint32_t spi_adc_cycles_to_hz(uint32_t cycles) {
#if defined(F_CPU) && F_CPU > 0
  if (cycles == 0u) {
    return 0u;
  }

  return (uint32_t)(((uint64_t)F_CPU + ((uint64_t)cycles / 2ull)) /
                    (uint64_t)cycles);
#else
  (void)cycles;
  return 0u;
#endif
}

static bool spi_adc_mode_clock_idle_high(uint8_t mode) {
  return (mode & 0x02u) != 0u;
}

static bool spi_adc_mode_sample_on_trailing_edge(uint8_t mode) {
  return (mode & 0x01u) != 0u;
}

static void spi_adc_write_gpio(gpio_type *port, uint16_t pin, bool high) {
  if (port == NULL || pin == 0u) {
    return;
  }

  if (high) {
    port->scr = pin;
  } else {
    port->clr = pin;
  }
}

static bool spi_adc_read_gpio(gpio_type *port, uint16_t pin) {
  if (port == NULL || pin == 0u) {
    return false;
  }

  return gpio_input_data_bit_read(port, pin) != RESET;
}

static void spi_adc_init_output_pin(gpio_type *port, uint16_t pin,
                                    bool initial_high) {
  gpio_init_type gpio_init_struct;

  analog_enable_gpio_clock(port);
  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_pins = pin;
  gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init(port, &gpio_init_struct);
  spi_adc_write_gpio(port, pin, initial_high);
}

static void spi_adc_init_input_pin(gpio_type *port, uint16_t pin) {
  gpio_init_type gpio_init_struct;

  analog_enable_gpio_clock(port);
  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_pins = pin;
  gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
  gpio_init_struct.gpio_pull = GPIO_PULL_UP;
  gpio_init(port, &gpio_init_struct);
}

static void spi_adc_init_gpio_buses(void) {
  for (uint32_t i = 0; i < M_ARRAY_SIZE(spi_adc_gpio_buses); i++) {
    const spi_adc_gpio_bus_t *bus = &spi_adc_gpio_buses[i];

    spi_adc_init_output_pin(bus->sck_port, bus->sck_pin, false);
    spi_adc_init_output_pin(bus->mosi_port, bus->mosi_pin, false);
    spi_adc_init_input_pin(bus->miso_port, bus->miso_pin);
  }
}

static const spi_adc_gpio_bus_t *spi_adc_get_bus(uint8_t bus_id) {
  if (bus_id >= M_ARRAY_SIZE(spi_adc_gpio_buses)) {
    board_error_handler();
  }

  return &spi_adc_gpio_buses[bus_id];
}

static void spi_adc_set_idle_clock(const spi_adc_gpio_bus_t *bus,
                                   uint8_t timing_mode) {
  spi_adc_write_gpio(bus->sck_port, bus->sck_pin,
                     spi_adc_mode_clock_idle_high(timing_mode));
}

static uint16_t spi_adc_transfer_word(const spi_adc_gpio_bus_t *bus,
                                      uint16_t tx_word,
                                      uint8_t timing_mode) {
  const bool idle_high = spi_adc_mode_clock_idle_high(timing_mode);
  const bool sample_on_trailing =
      spi_adc_mode_sample_on_trailing_edge(timing_mode);
  uint16_t rx_word = 0u;

  spi_adc_set_idle_clock(bus, timing_mode);

  for (uint16_t mask = 0x8000u; mask != 0u; mask >>= 1u) {
    if (!sample_on_trailing) {
      spi_adc_write_gpio(bus->mosi_port, bus->mosi_pin,
                         (tx_word & mask) != 0u);
      spi_adc_delay_cycles(spi_adc_half_period_cycles);
    }

    spi_adc_write_gpio(bus->sck_port, bus->sck_pin, !idle_high);
    spi_adc_delay_cycles(spi_adc_half_period_cycles);

    if (sample_on_trailing) {
      spi_adc_write_gpio(bus->mosi_port, bus->mosi_pin,
                         (tx_word & mask) != 0u);
      spi_adc_delay_cycles(spi_adc_half_period_cycles);
    } else {
      rx_word = (uint16_t)(((uint16_t)(rx_word << 1u)) |
                           (uint16_t)(spi_adc_read_gpio(bus->miso_port,
                                                        bus->miso_pin)
                                          ? 1u
                                          : 0u));
    }

    spi_adc_write_gpio(bus->sck_port, bus->sck_pin, idle_high);
    spi_adc_delay_cycles(spi_adc_half_period_cycles);

    if (sample_on_trailing) {
      rx_word = (uint16_t)(((uint16_t)(rx_word << 1u)) |
                           (uint16_t)(spi_adc_read_gpio(bus->miso_port,
                                                        bus->miso_pin)
                                          ? 1u
                                          : 0u));
    }
  }

  return rx_word;
}

static uint16_t spi_adc_device_frame_count(uint8_t device_index) {
  const uint8_t scan_count = spi_adc_device_scan_counts[device_index];
  if (scan_count > ADS7953_CHANNELS_PER_DEVICE) {
    board_error_handler();
  }

  return (uint16_t)scan_count + ADS7953_PIPELINE_FLUSH_FRAMES;
}

static uint16_t spi_adc_settings_command(void) {
  return ADS7953_COMMAND_MANUAL_SCAN | ADS7953_COMMAND_WRITE_BIT |
         ADS7953_COMMAND_RANGE_BIT;
}

static uint16_t spi_adc_scan_command(uint8_t channel) {
  return (uint16_t)(ADS7953_COMMAND_MANUAL_SCAN |
                    (uint16_t)(channel << ADS7953_COMMAND_CHANNEL_SHIFT));
}

static void spi_adc_write_word(const spi_adc_gpio_bus_t *bus,
                               uint8_t device_index, uint16_t word,
                               uint8_t timing_mode) {
  spi_adc_set_idle_clock(bus, timing_mode);
  spi_cs_select(&spi_adc_chip_selects[device_index]);
  spi_adc_delay_cycles(spi_adc_half_period_cycles);
  (void)spi_adc_transfer_word(bus, word, timing_mode);
  spi_adc_delay_cycles(spi_adc_half_period_cycles);
  spi_cs_deselect(&spi_adc_chip_selects[device_index]);
  spi_adc_delay_cycles(spi_adc_half_period_cycles);
}

static void analog_reset_scan_diagnostics_impl(void) {
  uint8_t active_device_count = 0;

  for (uint32_t i = 0; i < SPI_ADC_NUM_DEVICES; i++) {
    if (spi_adc_device_scan_counts[i] != 0u) {
      active_device_count++;
    }
  }

  memset(&analog_scan_diagnostics, 0, sizeof(analog_scan_diagnostics));
  analog_scan_diagnostics.mux_sample_delay_us = 0u;
  analog_scan_diagnostics.mux_step_count = 0u;
  analog_scan_diagnostics.active_bus_count = SPI_ADC_NUM_BUSES;
  analog_scan_diagnostics.active_device_count = active_device_count;
}

static void spi_adc_prepare_channel_masks(void) {
  for (uint32_t device = 0; device < SPI_ADC_NUM_DEVICES; device++) {
    uint16_t channel_mask = 0u;

    for (uint32_t i = 0; i < spi_adc_device_scan_counts[device]; i++) {
      const uint8_t channel = spi_adc_device_scan_channels[device][i];
      if (channel >= ADS7953_CHANNELS_PER_DEVICE) {
        board_error_handler();
      }
      if ((channel_mask & M_BIT(channel)) != 0u) {
        board_error_handler();
      }

      channel_mask |= (uint16_t)M_BIT(channel);
    }

    spi_adc_device_channel_masks[device] = channel_mask;
  }
}

static void spi_adc_init_bus_state(spi_adc_bus_state_t *bus,
                                   uint8_t logical_bus_index) {
  uint8_t write_index = 0u;

  memset(bus, 0, sizeof(*bus));
  bus->logical_bus_index = logical_bus_index;
  bus->bus_id = spi_adc_bus_ids[logical_bus_index];
  bus->timing_mode = 0u;
  (void)spi_adc_get_bus(bus->bus_id);

  for (uint8_t device = 0; device < SPI_ADC_NUM_DEVICES; device++) {
    if (spi_adc_device_bus[device] != bus->bus_id ||
        spi_adc_device_scan_counts[device] == 0u) {
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
}

static void spi_adc_store_response(uint8_t device_index, uint16_t rx_word) {
  const uint8_t channel = (uint8_t)((rx_word >> ADS7953_RESULT_CHANNEL_SHIFT) &
                                    ADS7953_RESULT_CHANNEL_MASK);
  const uint32_t raw_offset =
      (uint32_t)device_index * ADS7953_CHANNELS_PER_DEVICE;

  if (channel >= ADS7953_CHANNELS_PER_DEVICE) {
    analog_scan_diagnostics.bad_channel_id_count++;
    return;
  }

  if ((spi_adc_device_channel_masks[device_index] & M_BIT(channel)) == 0u) {
    analog_scan_diagnostics.bad_channel_id_count++;
    return;
  }

  spi_adc_scan_buffer[raw_offset + channel] = rx_word & ADC_MAX_VALUE;
}

static uint8_t spi_adc_extract_channel(uint16_t rx_word) {
  return (uint8_t)((rx_word >> ADS7953_RESULT_CHANNEL_SHIFT) &
                   ADS7953_RESULT_CHANNEL_MASK);
}

static uint8_t spi_adc_count_bits16(uint16_t value) {
  uint8_t count = 0u;

  while (value != 0u) {
    value &= (uint16_t)(value - 1u);
    count++;
  }

  return count;
}

static uint8_t spi_adc_score_probe_frames(const uint16_t *rx_words,
                                          uint8_t frame_count) {
  static const uint8_t probe_channels[SPI_ADC_MODE_PROBE_SCAN_COUNT] = {
      0u, 1u, 2u, 3u};
  uint16_t seen_mask = 0u;
  uint8_t score = 0u;

  for (uint8_t frame = 0; frame < frame_count; frame++) {
    const uint8_t channel = spi_adc_extract_channel(rx_words[frame]);
    if (channel < ADS7953_CHANNELS_PER_DEVICE) {
      seen_mask |= (uint16_t)M_BIT(channel);
    }

    if (frame > 0u && frame <= SPI_ADC_MODE_PROBE_SCAN_COUNT &&
        channel == probe_channels[frame - 1u]) {
      score = (uint8_t)(score + 2u);
    }
  }

  score = (uint8_t)(score + spi_adc_count_bits16(
                                (uint16_t)(seen_mask &
                                           ((1u << SPI_ADC_MODE_PROBE_SCAN_COUNT) -
                                            1u))));
  return score;
}

static uint8_t spi_adc_probe_device_timing(const spi_adc_gpio_bus_t *bus,
                                           uint8_t device_index,
                                           uint8_t timing_mode) {
  static const uint8_t probe_channels[SPI_ADC_MODE_PROBE_SCAN_COUNT] = {
      0u, 1u, 2u, 3u};
  uint16_t rx_words[SPI_ADC_MODE_PROBE_SCAN_COUNT +
                    ADS7953_PIPELINE_FLUSH_FRAMES] = {0};

  spi_adc_write_word(bus, device_index, spi_adc_settings_command(),
                     timing_mode);

  for (uint8_t frame = 0; frame < M_ARRAY_SIZE(rx_words); frame++) {
    const uint16_t tx_word =
        frame < SPI_ADC_MODE_PROBE_SCAN_COUNT
            ? spi_adc_scan_command(probe_channels[frame])
            : 0u;

    spi_adc_set_idle_clock(bus, timing_mode);
    spi_cs_select(&spi_adc_chip_selects[device_index]);
    spi_adc_delay_cycles(spi_adc_half_period_cycles);
    rx_words[frame] = spi_adc_transfer_word(bus, tx_word, timing_mode);
    spi_adc_delay_cycles(spi_adc_half_period_cycles);
    spi_cs_deselect(&spi_adc_chip_selects[device_index]);
    spi_adc_delay_cycles(spi_adc_half_period_cycles);
  }

  return spi_adc_score_probe_frames(rx_words, (uint8_t)M_ARRAY_SIZE(rx_words));
}

static uint8_t spi_adc_pick_timing_mode(const spi_adc_bus_state_t *bus_state) {
  const spi_adc_gpio_bus_t *bus = spi_adc_get_bus(bus_state->bus_id);
  uint8_t best_mode = 0u;
  uint8_t best_score = 0u;
  bool best_valid = false;

  for (uint8_t timing_mode = 0; timing_mode < SPI_ADC_TIMING_MODE_COUNT;
       timing_mode++) {
    uint8_t score = 0u;

    for (uint8_t device_slot = 0; device_slot < bus_state->num_devices;
         device_slot++) {
      score = (uint8_t)(score +
                        spi_adc_probe_device_timing(
                            bus, bus_state->device_indices[device_slot],
                            timing_mode));
    }

    if (!best_valid || score > best_score) {
      best_mode = timing_mode;
      best_score = score;
      best_valid = true;
    }
  }

  return best_mode;
}

static void spi_adc_scan_device(const spi_adc_bus_state_t *bus_state,
                                const spi_adc_gpio_bus_t *bus,
                                uint8_t device_index) {
  const uint8_t scan_count = spi_adc_device_scan_counts[device_index];
  const uint16_t frame_count = spi_adc_device_frame_count(device_index);
  const uint8_t timing_mode = bus_state->timing_mode;

  spi_adc_set_idle_clock(bus, timing_mode);
  memset(spi_adc_last_rx_words[device_index], 0,
         sizeof(spi_adc_last_rx_words[device_index]));

  for (uint16_t frame = 0; frame < frame_count; frame++) {
    uint16_t tx_word = 0u;
    uint16_t rx_word = 0u;

    if (frame < scan_count) {
      const uint8_t channel = spi_adc_device_scan_channels[device_index][frame];
      tx_word = spi_adc_scan_command(channel);
    }

    spi_cs_select(&spi_adc_chip_selects[device_index]);
    spi_adc_delay_cycles(spi_adc_half_period_cycles);
    rx_word = spi_adc_transfer_word(bus, tx_word, timing_mode);
    spi_adc_last_rx_words[device_index][frame] = rx_word;
    spi_adc_store_response(device_index, rx_word);
    spi_adc_delay_cycles(spi_adc_half_period_cycles);
    spi_cs_deselect(&spi_adc_chip_selects[device_index]);
    spi_adc_delay_cycles(spi_adc_half_period_cycles);
  }

  usb_bootstrap_pump();
}

static void spi_adc_run_scan_cycle(void) {
  const uint32_t start_cycles = board_cycle_count();

  memset(spi_adc_scan_buffer, 0, sizeof(spi_adc_scan_buffer));

  for (uint32_t logical_bus = 0; logical_bus < SPI_ADC_NUM_BUSES; logical_bus++) {
    const spi_adc_bus_state_t *bus_state = &spi_adc_buses[logical_bus];
    const spi_adc_gpio_bus_t *bus = spi_adc_get_bus(bus_state->bus_id);

    for (uint32_t device_slot = 0; device_slot < bus_state->num_devices;
         device_slot++) {
      spi_adc_scan_device(bus_state, bus, bus_state->device_indices[device_slot]);
    }
  }

  analog_scan_store_samples(spi_adc_scan_buffer, 0);

  const uint32_t elapsed_cycles = board_cycle_count() - start_cycles;
  analog_scan_diagnostics.scan_count++;
  analog_scan_diagnostics.last_scan_cycles = elapsed_cycles;
  analog_scan_diagnostics.last_scan_us = spi_adc_cycles_to_us(elapsed_cycles);
  analog_scan_diagnostics.estimated_scan_hz =
      spi_adc_cycles_to_hz(elapsed_cycles);
  analog_scan_diagnostics.last_bus_completion_skew_cycles = 0u;
  if (elapsed_cycles > analog_scan_diagnostics.max_scan_cycles) {
    analog_scan_diagnostics.max_scan_cycles = elapsed_cycles;
    analog_scan_diagnostics.max_scan_us = analog_scan_diagnostics.last_scan_us;
  }
}

void analog_init(void) {
#if DIGITAL_NUM_INPUTS > 0
  analog_init_digital_inputs();
#endif

  analog_scan_reset();
  analog_reset_scan_diagnostics_impl();
  spi_adc_half_period_cycles = spi_adc_pick_half_period_cycles();
  spi_adc_prepare_channel_masks();
  spi_adc_init_gpio_buses();

  for (uint32_t i = 0; i < SPI_ADC_NUM_DEVICES; i++) {
    spi_adc_chip_selects[i].port = spi_adc_device_cs_ports[i];
    spi_adc_chip_selects[i].pin = spi_adc_device_cs_pins[i];
    spi_adc_chip_selects[i].active_low = true;
    spi_cs_init(&spi_adc_chip_selects[i]);
  }

  for (uint32_t i = 0; i < SPI_ADC_NUM_BUSES; i++) {
    spi_adc_init_bus_state(&spi_adc_buses[i], (uint8_t)i);
  }

  for (uint32_t logical_bus = 0; logical_bus < SPI_ADC_NUM_BUSES; logical_bus++) {
    spi_adc_bus_state_t *bus_state = &spi_adc_buses[logical_bus];
    bus_state->timing_mode = spi_adc_pick_timing_mode(bus_state);
    analog_scan_diagnostics.reserved |=
        (uint16_t)(bus_state->timing_mode << (logical_bus * 8u));
  }

  for (uint32_t logical_bus = 0; logical_bus < SPI_ADC_NUM_BUSES; logical_bus++) {
    const spi_adc_bus_state_t *bus_state = &spi_adc_buses[logical_bus];
    const spi_adc_gpio_bus_t *bus = spi_adc_get_bus(bus_state->bus_id);

    for (uint32_t device_slot = 0; device_slot < bus_state->num_devices;
         device_slot++) {
      spi_adc_write_word(bus, bus_state->device_indices[device_slot],
                         spi_adc_settings_command(), bus_state->timing_mode);
    }
  }

  spi_adc_run_scan_cycle();
}

void analog_task(void) { spi_adc_run_scan_cycle(); }

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

const analog_scan_diagnostics_t *analog_get_scan_diagnostics(void) {
  return &analog_scan_diagnostics;
}

void analog_reset_scan_diagnostics(void) {
  analog_reset_scan_diagnostics_impl();
}

uint16_t analog_get_mux_sample_delay_us(void) { return 0u; }

bool analog_set_mux_sample_delay_us(uint16_t delay_us) {
  (void)delay_us;
  return false;
}

uint16_t analog_debug_frame_count(void) {
  return (uint16_t)(SPI_ADC_NUM_DEVICES * ADS7953_MAX_FRAMES_PER_DEVICE);
}

uint16_t analog_read_debug_frame(uint8_t index) {
  const uint8_t frames_per_device = ADS7953_MAX_FRAMES_PER_DEVICE;
  const uint8_t device_index = (uint8_t)(index / frames_per_device);
  const uint8_t frame_index = (uint8_t)(index % frames_per_device);

  if (device_index >= SPI_ADC_NUM_DEVICES) {
    return 0u;
  }

  return spi_adc_last_rx_words[device_index][frame_index];
}

#endif
