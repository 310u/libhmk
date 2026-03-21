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

#include "profile_runtime.h"

#include <string.h>

#include "eeconfig.h"
#include "joystick.h"
#include "layout.h"
#include "rgb.h"

void profile_runtime_apply_current(void) {
  layout_load_advanced_keys();
#if defined(RGB_ENABLED)
  memcpy(rgb_get_config(), &CURRENT_PROFILE.rgb_config, sizeof(rgb_config_t));
  rgb_apply_config();
#endif
#if defined(JOYSTICK_ENABLED)
  joystick_apply_config(CURRENT_PROFILE.joystick_config);
#endif
}

void profile_runtime_reload_current(void) {
  layout_reset_runtime_state();
  profile_runtime_apply_current();
}
