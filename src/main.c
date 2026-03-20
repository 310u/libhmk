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

#include "advanced_keys.h"
#include "commands.h"
#include "crc32.h"
#include "deferred_actions.h"
#include "eeconfig.h"
#include "encoder.h"
#include "hardware/hardware.h"
#include "hid.h"
#include "joystick.h"
#include "layout.h"
#include "matrix.h"
#include "rgb.h"
#include "tusb.h"
#include "wear_leveling.h"
#include "xinput.h"
#include "slider.h"

int main(void) {
  // Initialize the hardware
  board_init();
  timer_init();
  crc32_init();
  flash_init();

  // Initialize the persistent configuration
  wear_leveling_init();
  eeconfig_init();
#if defined(RECOVERY_RESET_CURRENT_PROFILE_RGB) && defined(RGB_ENABLED)
  (void)eeconfig_reset_profile_rgb(eeconfig->current_profile);
#endif

  // Initialize the core modules
  analog_init();
  matrix_init();
#if defined(RGB_ENABLED)
  rgb_init();
#endif
  hid_init();
  deferred_action_init();
  advanced_key_init();
  xinput_init();
  layout_init();
  encoder_init();
#if defined(JOYSTICK_ENABLED)
  joystick_init();
#endif
  slider_init();
  command_init();

  tud_init(BOARD_TUD_RHPORT);

  while (1) {
    tud_task();

    analog_task();
    matrix_scan();
    encoder_task();
    layout_task();
#if defined(RGB_ENABLED)
    rgb_task();
#endif
#if defined(JOYSTICK_ENABLED)
    joystick_task();
#endif
    slider_task();
    xinput_task();
  }

  return 0;
}
