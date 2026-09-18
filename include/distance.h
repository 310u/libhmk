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

#pragma once

#include "common.h"

//--------------------------------------------------------------------+
// ADC to Distance Conversion
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// Distance Curve
//--------------------------------------------------------------------+

#define DISTANCE_CURVE_POINTS_MAX 9

/**
 * @brief Single switch-travel curve used to map normalized ADC to normalized
 *        physical distance.
 *
 * Points are stored in normalized coordinates where both axes span [0, 255].
 * The ADC axis represents (adc - rest) / (bottom - rest); the distance axis
 * represents mm / total_travel * 255.
 *
 * num_points == 0 means the identity curve (linear), which preserves the
 * previous behavior when no curve is configured.
 */
typedef struct __attribute__((packed)) {
  uint8_t num_points;
  uint16_t total_travel_um;
  struct __attribute__((packed)) {
    uint8_t adc;
    uint8_t dist;
  } points[DISTANCE_CURVE_POINTS_MAX];
} distance_curve_t;

/**
 * @brief Convert ADC value to distance with an optional switch-travel curve.
 *
 * Maps the ADC range between rest and bottom-out values to [0, 255]. When
 * curve is NULL or empty, the mapping is linear.
 *
 * @param adc ADC value
 * @param adc_rest_value ADC value when the key is fully released
 * @param adc_bottom_out_value ADC value when the key is fully pressed
 * @param curve Optional switch-travel curve
 *
 * @return Distance in the range [0, 255]
 */
__attribute__((always_inline)) static inline uint8_t
adc_to_distance_with_curve(uint16_t adc, uint16_t adc_rest_value,
                           uint16_t adc_bottom_out_value,
                           const distance_curve_t *curve) {
  // Handle edge cases. Runtime rest tracking and noisy bottom-out samples can
  // temporarily shrink the effective span until the next full stroke.
  if ((adc <= adc_rest_value) | (adc_rest_value >= adc_bottom_out_value))
    return 0;
  if (adc >= adc_bottom_out_value)
    return 255;

  const uint32_t span = (uint32_t)(adc_bottom_out_value - adc_rest_value);
  const uint32_t offset = (uint32_t)(adc - adc_rest_value);

  // Normalize ADC value to [0, 255] with round-to-nearest.
  const uint8_t t = (uint8_t)((offset * 255u + span / 2u) / span);

  if (curve == NULL || curve->num_points == 0)
    return t;

  const uint8_t n = curve->num_points;
  if (n > DISTANCE_CURVE_POINTS_MAX)
    return t;

  if (t <= curve->points[0].adc)
    return curve->points[0].dist;
  if (t >= curve->points[n - 1].adc)
    return curve->points[n - 1].dist;

  for (uint8_t i = 0; i + 1 < n; i++) {
    const uint8_t a0 = curve->points[i].adc;
    const uint8_t a1 = curve->points[i + 1].adc;
    if (t >= a0 && t <= a1) {
      const uint8_t d0 = curve->points[i].dist;
      const uint8_t d1 = curve->points[i + 1].dist;
      const uint32_t seg = (uint32_t)(a1 - a0);
      if (seg == 0)
        return d0;
      const uint32_t off = (uint32_t)(t - a0);
      return (uint8_t)((off * (uint32_t)(d1 - d0) + seg / 2u) / seg + d0);
    }
  }

  return curve->points[n - 1].dist;
}

/**
 * @brief Convert ADC value to distance using the identity (linear) curve.
 *
 * Kept for callers that do not need a per-switch curve, including native
 * unit tests.
 */
__attribute__((always_inline)) static inline uint8_t
adc_to_distance(uint16_t adc, uint16_t adc_rest_value,
                uint16_t adc_bottom_out_value) {
  return adc_to_distance_with_curve(adc, adc_rest_value, adc_bottom_out_value,
                                    NULL);
}
