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

void timer_init(void) {}

uint32_t timer_read(void) { return HAL_GetTick(); }

uint32_t timer_read_us(void) {
  uint32_t m;
  uint32_t val;
  uint32_t load;
  do {
    m = HAL_GetTick();
    val = SysTick->VAL;
  } while (m != HAL_GetTick());
  load = SysTick->LOAD + 1u;
  if (load == 0u) {
    return m * 1000u;
  }
  uint32_t elapsed_cycles = load - val;
  uint32_t us_in_ms = (uint32_t)(((uint64_t)elapsed_cycles * 1000u) / load);
  return m * 1000u + us_in_ms;
}

