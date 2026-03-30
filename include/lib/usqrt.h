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
// Fast Integer Square Root
// Adapted from https://www.azillionmonkeys.com/qed/ulerysqroot.pdf
//--------------------------------------------------------------------+

static inline void usqrt16_step(uint16_t *x, uint16_t *g, uint8_t n) {
  const unsigned int shift = (unsigned int)n;
  const uint16_t t =
      (uint16_t)(((uint32_t)(*g) << (shift + 1u)) + (UINT32_C(1) << (shift << 1u)));
  if (*x >= t) {
    *g = (uint16_t)(*g + (uint16_t)(UINT32_C(1) << shift));
    *x = (uint16_t)(*x - t);
  }
}

static inline uint16_t usqrt16(uint16_t x) {
  uint16_t g = 0;

  usqrt16_step(&x, &g, 7u);
  usqrt16_step(&x, &g, 6u);
  usqrt16_step(&x, &g, 5u);
  usqrt16_step(&x, &g, 4u);
  usqrt16_step(&x, &g, 3u);
  usqrt16_step(&x, &g, 2u);
  usqrt16_step(&x, &g, 1u);
  usqrt16_step(&x, &g, 0u);

  return g;
}

static inline void usqrt32_step(uint32_t *x, uint32_t *g, uint8_t n) {
  const unsigned int shift = (unsigned int)n;
  const uint32_t t = (*g << (shift + 1u)) + (UINT32_C(1) << (shift << 1u));
  if (*x >= t) {
    *g += UINT32_C(1) << shift;
    *x -= t;
  }
}

static inline uint32_t usqrt32(uint32_t x) {
  uint32_t g = 0;

  usqrt32_step(&x, &g, 15u);
  usqrt32_step(&x, &g, 14u);
  usqrt32_step(&x, &g, 13u);
  usqrt32_step(&x, &g, 12u);
  usqrt32_step(&x, &g, 11u);
  usqrt32_step(&x, &g, 10u);
  usqrt32_step(&x, &g, 9u);
  usqrt32_step(&x, &g, 8u);
  usqrt32_step(&x, &g, 7u);
  usqrt32_step(&x, &g, 6u);
  usqrt32_step(&x, &g, 5u);
  usqrt32_step(&x, &g, 4u);
  usqrt32_step(&x, &g, 3u);
  usqrt32_step(&x, &g, 2u);
  usqrt32_step(&x, &g, 1u);
  usqrt32_step(&x, &g, 0u);

  return g;
}
