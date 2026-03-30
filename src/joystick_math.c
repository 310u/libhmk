#include "joystick_math.h"

#include <math.h>

// These helpers intentionally use libm. If a future backend cannot afford
// floating-point work in the joystick scan path, add a fixed-point
// implementation before enabling joystick support there.

#define JOYSTICK_CIRCULAR_TARGET_MAGNITUDE 127u
#define JOYSTICK_FULL_CIRCLE_RADIANS 6.28318530718f
#define JOYSTICK_OUTPUT_FP_SHIFT 8
#define JOYSTICK_OUTPUT_FP_ONE (1L << JOYSTICK_OUTPUT_FP_SHIFT)

static float joystick_boundary_sector_from_vector_fp(int32_t x_fp,
                                                     int32_t y_fp) {
  float angle = atan2f((float)y_fp, (float)x_fp);

  if (angle < 0.0f) {
    angle += JOYSTICK_FULL_CIRCLE_RADIANS;
  }

  return angle * ((float)JOYSTICK_RADIAL_BOUNDARY_SECTORS /
                  JOYSTICK_FULL_CIRCLE_RADIANS);
}

static uint8_t joystick_wrap_boundary_index(int16_t index) {
  int16_t wrapped = index % (int16_t)JOYSTICK_RADIAL_BOUNDARY_SECTORS;
  if (wrapped < 0) {
    wrapped += (int16_t)JOYSTICK_RADIAL_BOUNDARY_SECTORS;
  }

  return (uint8_t)wrapped;
}

static float joystick_boundary_value(const uint8_t *boundaries, int16_t index) {
  float value = (float)boundaries[joystick_wrap_boundary_index(index)];

  if (value <= 0.0f) {
    return JOYSTICK_RADIAL_BOUNDARY_DEFAULT;
  }

  return value;
}

static float joystick_monotone_boundary_tangent(float previous, float current,
                                                float next) {
  float left_delta = current - previous;
  float right_delta = next - current;

  if (left_delta == 0.0f || right_delta == 0.0f ||
      ((left_delta < 0.0f) != (right_delta < 0.0f))) {
    return 0.0f;
  }

  float denominator = left_delta + right_delta;
  if (denominator == 0.0f) {
    return 0.0f;
  }

  return (2.0f * left_delta * right_delta) / denominator;
}

static float joystick_monotone_boundary_interpolate(float previous,
                                                    float current, float next,
                                                    float following,
                                                    float fraction) {
  if (fraction <= 0.0f) {
    return current;
  }
  if (fraction >= 1.0f) {
    return next;
  }

  float tangent_current =
      joystick_monotone_boundary_tangent(previous, current, next);
  float tangent_next =
      joystick_monotone_boundary_tangent(current, next, following);
  float fraction_sq = fraction * fraction;
  float fraction_cu = fraction_sq * fraction;
  float interpolated =
      ((2.0f * fraction_cu) - (3.0f * fraction_sq) + 1.0f) * current +
      (fraction_cu - (2.0f * fraction_sq) + fraction) * tangent_current +
      ((-2.0f * fraction_cu) + (3.0f * fraction_sq)) * next +
      (fraction_cu - fraction_sq) * tangent_next;
  float min_boundary = current < next ? current : next;
  float max_boundary = current > next ? current : next;

  if (interpolated < min_boundary) {
    return min_boundary;
  }
  if (interpolated > max_boundary) {
    return max_boundary;
  }

  return interpolated;
}

float joystick_boundary_lookup(const uint8_t *boundaries, float sector) {
  int16_t lower_index = (int16_t)floorf(sector);
  float fraction = sector - floorf(sector);
  float previous =
      joystick_boundary_value(boundaries, (int16_t)(lower_index - 1));
  float lower = joystick_boundary_value(boundaries, lower_index);
  float upper =
      joystick_boundary_value(boundaries, (int16_t)(lower_index + 1));
  float following =
      joystick_boundary_value(boundaries, (int16_t)(lower_index + 2));

  return joystick_monotone_boundary_interpolate(previous, lower, upper,
                                                following, fraction);
}

void joystick_apply_circular_correction_fp(const uint8_t *boundaries,
                                           int32_t *x_fp, int32_t *y_fp) {
  if (*x_fp == 0 && *y_fp == 0) {
    return;
  }

  float sector = joystick_boundary_sector_from_vector_fp(*x_fp, *y_fp);
  float observed_boundary = joystick_boundary_lookup(boundaries, sector);
  if (observed_boundary < 1.0f) {
    return;
  }

  float scale = (float)JOYSTICK_CIRCULAR_TARGET_MAGNITUDE / observed_boundary;
  *x_fp = (int32_t)lroundf((float)(*x_fp) * scale);
  *y_fp = (int32_t)lroundf((float)(*y_fp) * scale);
}

void joystick_apply_radial_deadzone_fp(int32_t *x_fp, int32_t *y_fp,
                                       uint8_t deadzone) {
  float x = (float)(*x_fp) / (float)JOYSTICK_OUTPUT_FP_ONE;
  float y = (float)(*y_fp) / (float)JOYSTICK_OUTPUT_FP_ONE;
  float magnitude = hypotf(x, y);
  if (magnitude <= 0.0f) {
    return;
  }

  if (deadzone >= 255u) {
    *x_fp = 0;
    *y_fp = 0;
    return;
  }

  float magnitude_norm =
      magnitude * 255.0f / (float)JOYSTICK_CIRCULAR_TARGET_MAGNITUDE;
  if (magnitude_norm > 255.0f) {
    magnitude_norm = 255.0f;
  }
  if (magnitude_norm <= (float)deadzone) {
    *x_fp = 0;
    *y_fp = 0;
    return;
  }

  float scaled_norm = ((magnitude_norm - (float)deadzone) * 255.0f) /
                      (255.0f - (float)deadzone);
  float scale = scaled_norm / magnitude_norm;
  *x_fp = (int32_t)lroundf((float)(*x_fp) * scale);
  *y_fp = (int32_t)lroundf((float)(*y_fp) * scale);
}
