#pragma once

#include "common.h"

// CPI range for the PAW3395 trackball sensor. Values are in counts-per-inch.
#define TRACKBALL_CPI_STEP 50u
#define TRACKBALL_CPI_MIN 50u
#define TRACKBALL_CPI_MAX 26000u
#define TRACKBALL_CPI_DEFAULT 1600u

// Mouse motion configuration
#define TRACKBALL_MOUSE_PRESET_COUNT 4u
#define TRACKBALL_MOUSE_SPEED_DEFAULT 10u
#define TRACKBALL_MOUSE_ACCELERATION_DEFAULT 255u
#define TRACKBALL_SMOOTHING_DEFAULT 0u
#define TRACKBALL_SMOOTHING_MAX 7u

typedef struct __attribute__((packed)) {
  uint8_t mouse_speed;        // 1-255, linear speed multiplier
  uint8_t mouse_acceleration; // 1-255, 255 = strongest acceleration curve
} trackball_mouse_preset_t;

typedef struct __attribute__((packed)) {
  uint16_t cpi;     // Counts per inch (50 to 26000, in 50-count steps)
  bool enabled;     // When false, motion output is suppressed (sensor stays on)
  bool invert_x;    // Invert the X axis
  bool invert_y;    // Invert the Y axis
  bool swap_xy;     // Swap the X and Y axes
  uint8_t mouse_speed;            // Active linear speed multiplier (1-255)
  uint8_t mouse_acceleration;     // Active acceleration curve strength (1-255)
  uint8_t active_mouse_preset;    // Index of the currently active preset
  trackball_mouse_preset_t mouse_presets[TRACKBALL_MOUSE_PRESET_COUNT];
  uint8_t smoothing; // Exponential smoothing exponent, 0 = off, 1-7 = stronger
  uint8_t reserved[2];
} trackball_config_t;

static inline trackball_mouse_preset_t trackball_make_mouse_preset(uint8_t speed,
                                                                   uint8_t accel) {
  return (trackball_mouse_preset_t){
      .mouse_speed = speed == 0u ? TRACKBALL_MOUSE_SPEED_DEFAULT : speed,
      .mouse_acceleration =
          accel == 0u ? TRACKBALL_MOUSE_ACCELERATION_DEFAULT : accel,
  };
}

static inline void trackball_fill_default_mouse_presets(
    trackball_mouse_preset_t presets[TRACKBALL_MOUSE_PRESET_COUNT]) {
  presets[0] = trackball_make_mouse_preset(10u, 255u);
  presets[1] = trackball_make_mouse_preset(20u, 200u);
  presets[2] = trackball_make_mouse_preset(5u, 255u);
  presets[3] = trackball_make_mouse_preset(30u, 150u);
}

static inline void
trackball_init_default_config(trackball_config_t *config) {
  memset(config, 0, sizeof(*config));
  config->cpi = TRACKBALL_CPI_DEFAULT;
  config->enabled = true;
  config->mouse_speed = TRACKBALL_MOUSE_SPEED_DEFAULT;
  config->mouse_acceleration = TRACKBALL_MOUSE_ACCELERATION_DEFAULT;
  config->active_mouse_preset = 0u;
  config->smoothing = TRACKBALL_SMOOTHING_DEFAULT;
  trackball_fill_default_mouse_presets(config->mouse_presets);
}

static inline trackball_config_t
trackball_normalize_config(trackball_config_t config) {
  if (config.cpi < TRACKBALL_CPI_MIN) {
    config.cpi = TRACKBALL_CPI_MIN;
  } else if (config.cpi > TRACKBALL_CPI_MAX) {
    config.cpi = TRACKBALL_CPI_MAX;
  }
  config.cpi = (uint16_t)(((uint32_t)config.cpi + TRACKBALL_CPI_STEP / 2u) /
                          TRACKBALL_CPI_STEP * TRACKBALL_CPI_STEP);
  if (config.cpi < TRACKBALL_CPI_MIN) {
    config.cpi = TRACKBALL_CPI_MIN;
  }

  if (config.mouse_speed == 0u) {
    config.mouse_speed = TRACKBALL_MOUSE_SPEED_DEFAULT;
  }
  if (config.mouse_acceleration == 0u) {
    config.mouse_acceleration = TRACKBALL_MOUSE_ACCELERATION_DEFAULT;
  }
  if (config.active_mouse_preset >= TRACKBALL_MOUSE_PRESET_COUNT) {
    config.active_mouse_preset = 0u;
  }

  const trackball_mouse_preset_t fallback =
      trackball_make_mouse_preset(config.mouse_speed, config.mouse_acceleration);
  for (uint8_t i = 0; i < TRACKBALL_MOUSE_PRESET_COUNT; i++) {
    if (config.mouse_presets[i].mouse_speed == 0u) {
      config.mouse_presets[i].mouse_speed = fallback.mouse_speed;
    }
    if (config.mouse_presets[i].mouse_acceleration == 0u) {
      config.mouse_presets[i].mouse_acceleration = fallback.mouse_acceleration;
    }
  }

  if (config.smoothing > TRACKBALL_SMOOTHING_MAX) {
    config.smoothing = TRACKBALL_SMOOTHING_MAX;
  }

  trackball_mouse_preset_t *active_preset =
      &config.mouse_presets[config.active_mouse_preset];
  if (config.mouse_speed != active_preset->mouse_speed ||
      config.mouse_acceleration != active_preset->mouse_acceleration) {
    *active_preset =
        trackball_make_mouse_preset(config.mouse_speed, config.mouse_acceleration);
  }
  config.mouse_speed = active_preset->mouse_speed;
  config.mouse_acceleration = active_preset->mouse_acceleration;

  return config;
}

void trackball_init(void);
void trackball_task(void);
void trackball_select_mouse_preset(trackball_config_t *config, uint8_t preset);

// トラックボールの状態取得用（Diagnostics向け）
typedef struct {
  bool enabled;
  uint16_t current_cpi;
  int16_t last_dx;
  int16_t last_dy;
} trackball_diagnostic_state_t;

void trackball_get_state(trackball_diagnostic_state_t *state);

trackball_config_t trackball_get_config(void);
void trackball_apply_config(trackball_config_t config);
void trackball_set_config(trackball_config_t config);

// CPI トグル/インクリメント API
void trackball_increase_cpi(void);
void trackball_decrease_cpi(void);
void trackball_select_next_preset(void);
