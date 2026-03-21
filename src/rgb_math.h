#pragma once

#include <math.h>

#include "rgb.h"

static inline uint8_t qadd8(uint8_t a, uint8_t b) {
  const uint16_t res = (uint16_t)a + b;
  return (res > 255u) ? 255u : (uint8_t)res;
}

static inline uint8_t qsub8(uint8_t a, uint8_t b) {
  return (a > b) ? (uint8_t)(a - b) : 0u;
}

static inline uint8_t scale8(uint8_t value, uint8_t scale) {
  return (uint8_t)(((uint16_t)value * (uint16_t)(scale + 1u)) >> 8);
}

static inline uint16_t scale16by8(uint16_t value, uint8_t scale) {
  return (uint16_t)(((uint32_t)value * (uint32_t)(scale + 1u)) >> 8);
}

static inline uint8_t abs8(int16_t v) {
  return (uint8_t)((v < 0) ? -v : v);
}

static inline uint8_t sin8(uint8_t theta) {
  float rad = (float)theta * (2.0f * (float)M_PI / 256.0f);
  int16_t s = (int16_t)(sinf(rad) * 127.0f) + 128;
  if (s < 0) {
    s = 0;
  }
  if (s > 255) {
    s = 255;
  }
  return (uint8_t)s;
}

static inline uint8_t cos8(uint8_t theta) { return sin8((uint8_t)(theta + 64u)); }
