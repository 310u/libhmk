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

#include "xinput.h"

#include "device/usbd_pvt.h"
#include "eeconfig.h"
#include "joystick.h"
#include "layout.h"
#include "lib/bitmap.h"
#include "lib/usqrt.h"
#include "matrix.h"
#include "tusb.h"
#include "usb_descriptors.h"

/**
 * @brief Convert square joystick coordinates to circular coordinates
 *
 * @param x X coordinate
 * @param y Y coordinate
 *
 * @return X in circular coordinates
 */
static uint8_t square_to_circular(uint8_t x, uint8_t y) {
  return (uint16_t)x * usqrt16(255 * 255 - (((uint16_t)y * y) >> 1)) / 255;
}

/**
 * @brief Apply the analog curve to the analog value
 *
 * We assume that the X coordinates are strictly increasing.
 *
 * @param value Analog value
 * @param[out] is_key_end_deadzone Whether the analog value is in the key end
 * deadzone
 *
 * @return Processed analog value
 */
static uint8_t apply_analog_curve(uint8_t value, bool *is_key_end_deadzone) {
  const uint8_t (*curve)[2] = CURRENT_PROFILE.gamepad_options.analog_curve;

  *is_key_end_deadzone = (value > curve[3][0]);
  if (*is_key_end_deadzone)
    // Key end deadzone
    return 255;

  if (value <= curve[0][0])
    // Key start deadzone
    return 0;

  // Find the segment in the curve where the value falls
  uint8_t i = 0;
  for (; i < 3; i++) {
    if (curve[i + 1][0] >= value)
      break;
  }

  const int16_t x1 = curve[i][0], y1 = curve[i][1];
  const int16_t x2 = curve[i + 1][0], y2 = curve[i + 1][1];

  return y1 + (y2 - y1) * (value - x1) / (x2 - x1);
}

// Mapping for digital gamepad buttons to XInput button bitmasks
static const uint16_t keycode_to_bm[] = {
    [GP_BUTTON_A] = XINPUT_BUTTON_A,
    [GP_BUTTON_B] = XINPUT_BUTTON_B,
    [GP_BUTTON_X] = XINPUT_BUTTON_X,
    [GP_BUTTON_Y] = XINPUT_BUTTON_Y,
    [GP_BUTTON_UP] = XINPUT_BUTTON_UP,
    [GP_BUTTON_DOWN] = XINPUT_BUTTON_DOWN,
    [GP_BUTTON_LEFT] = XINPUT_BUTTON_LEFT,
    [GP_BUTTON_RIGHT] = XINPUT_BUTTON_RIGHT,
    [GP_BUTTON_START] = XINPUT_BUTTON_START,
    [GP_BUTTON_BACK] = XINPUT_BUTTON_BACK,
    [GP_BUTTON_HOME] = XINPUT_BUTTON_HOME,
    [GP_BUTTON_LS] = XINPUT_BUTTON_LS,
    [GP_BUTTON_RS] = XINPUT_BUTTON_RS,
    [GP_BUTTON_LB] = XINPUT_BUTTON_LB,
    [GP_BUTTON_RB] = XINPUT_BUTTON_RB,
};

// Negative and positive axes for joysticks
static const uint8_t joystick_axes[][2] = {
    {GP_BUTTON_LS_LEFT, GP_BUTTON_LS_RIGHT}, // lx
    {GP_BUTTON_LS_DOWN, GP_BUTTON_LS_UP},    // ly
    {GP_BUTTON_RS_LEFT, GP_BUTTON_RS_RIGHT}, // rx
    {GP_BUTTON_RS_DOWN, GP_BUTTON_RS_UP},    // ry
};

// Endpoints for XInput communication
static uint8_t endpoint_in;
static uint8_t endpoint_out;

// We track key press states independently of the layout module in case
// layout processing is disabled for some keys.
static bitmap_t key_press_states[BITMAP_SIZE(NUM_KEYS)] = {0};
// Track maximum analog values for analog buttons
// (2 joysticks * 4 directions + 2 triggers)
static uint16_t analog_states[10];

// Access analog states using the button index
#define ANALOG_STATE(button) analog_states[(button) - GP_BUTTON_LS_UP]

static xinput_report_t report = {.report_size = sizeof(xinput_report_t)};

void xinput_init(void) {}

void xinput_process(uint8_t key) {
  const key_state_t *k = &key_matrix[key];
  const uint8_t keycode = CURRENT_PROFILE.gamepad_buttons[key];

  if (keycode == GP_BUTTON_NONE)
    return;

  switch (keycode) {
  case GP_BUTTON_A ... GP_BUTTON_RB: {
    const bool last_key_press = bitmap_get(key_press_states, key);

    if (k->is_pressed & !last_key_press)
      // Key press event
      report.buttons |= keycode_to_bm[keycode];
    else if (!k->is_pressed & last_key_press)
      // Key release event
      report.buttons &= ~keycode_to_bm[keycode];

    // Finally, update the key state
    bitmap_set(key_press_states, key, k->is_pressed);
    break;
  }
  case GP_BUTTON_LS_UP ... GP_BUTTON_RT: {
    // Update the maximum analog value for the analog button
    ANALOG_STATE(keycode) = M_MAX(ANALOG_STATE(keycode), k->distance);
    break;
  }
  default: {
    break;
  }
  }
}

void xinput_task(void) {
  static xinput_report_t last_report = {.report_size = sizeof(xinput_report_t)};

  // Inject slider override if Gamepad Mode is active
  if (eeconfig->options.slider_mode == 2) {
    uint8_t slider_val = key_matrix[NUM_KEYS - 1].distance;
    uint8_t gp_btn = GP_BUTTON_NONE;
    switch (eeconfig->options.slider_action) {
      case 0: gp_btn = GP_BUTTON_LS_UP; break;
      case 1: gp_btn = GP_BUTTON_LS_DOWN; break;
      case 2: gp_btn = GP_BUTTON_LS_LEFT; break;
      case 3: gp_btn = GP_BUTTON_LS_RIGHT; break;
      case 4: gp_btn = GP_BUTTON_RS_UP; break;
      case 5: gp_btn = GP_BUTTON_RS_DOWN; break;
      case 6: gp_btn = GP_BUTTON_RS_LEFT; break;
      case 7: gp_btn = GP_BUTTON_RS_RIGHT; break;
      case 8: gp_btn = GP_BUTTON_LT; break;
      case 9: gp_btn = GP_BUTTON_RT; break;
      default: break;
    }
    if (gp_btn != GP_BUTTON_NONE) {
      ANALOG_STATE(gp_btn) = M_MAX(ANALOG_STATE(gp_btn), slider_val);
    }
  }

  bool is_key_end_deadzone = false;
  // Update trigger states in the report
  report.lz =
      apply_analog_curve(ANALOG_STATE(GP_BUTTON_LT), &is_key_end_deadzone);
  report.rz =
      apply_analog_curve(ANALOG_STATE(GP_BUTTON_RT), &is_key_end_deadzone);

  // lx, ly, rx, ry
  uint16_t joystick_states[4] = {0};

  // Combine joystick axes based on the configuration
  for (uint32_t i = 0; i < 4; i++) {
    const uint8_t neg_axis = joystick_axes[i][0];
    const uint8_t pos_axis = joystick_axes[i][1];

    if (CURRENT_PROFILE.gamepad_options.snappy_joystick)
      // For snappy joystick, we use the maximum value of opposite axes.
      joystick_states[i] =
          M_MAX(ANALOG_STATE(neg_axis), ANALOG_STATE(pos_axis));
    else
      // Otherwise, we combine the opposite axes.
      joystick_states[i] =
          abs((int16_t)ANALOG_STATE(pos_axis) - ANALOG_STATE(neg_axis));
  }

  // Apply the analog curve to joystick states
  for (uint32_t i = 0; i < 2; i++) {
    uint16_t *state = &joystick_states[i * 2];

    uint32_t x = state[0], y = state[1];
    const uint32_t magnitude = usqrt32(x * x + y * y);
    if (magnitude == 0)
      // If magnitude is zero, skip analog curve processing
      continue;

    // Calculate the maximum magnitude for the joystick vector
    const uint32_t max_x = x > y ? 255 : x * 255 / y;
    const uint32_t max_y = y > x ? 255 : y * 255 / x;
    const uint32_t max_magnitude = usqrt32(max_x * max_x + max_y * max_y);
    // Apply the analog curve to the joystick magnitude. The magnitude is
    // scaled to [0, 255] range.
    const uint32_t new_magnitude = apply_analog_curve(
        magnitude * 255 / max_magnitude, &is_key_end_deadzone);

    if (is_key_end_deadzone) {
      // If the joystick is in the key end deadzone, we snap the axes to
      // maximum analog value.
      x = x == 0 ? 0 : 255;
      y = y == 0 ? 0 : 255;
    } else {
      // Otherwise, scale the joystick states to the new magnitude
      // We scale the maximum vector instead of the joystick vector to
      // prevent the analog values from exceeding the maximum range due to
      // approximation errors.
      x = max_x * new_magnitude / 255;
      y = max_y * new_magnitude / 255;
    }

    if (!CURRENT_PROFILE.gamepad_options.square_joystick) {
      // Convert square joystick coordinates to circular coordinates
      state[0] = square_to_circular(x, y);
      state[1] = square_to_circular(y, x);
    } else {
      // Otherwise, use the original values
      state[0] = x;
      state[1] = y;
    }

    if (is_sniper_active) {
      state[0] = (uint16_t)state[0] * eeconfig->options.sniper_mode_multiplier / 255;
      state[1] = (uint16_t)state[1] * eeconfig->options.sniper_mode_multiplier / 255;
    }
  }

  // Update joystick states in the report
  for (uint32_t i = 0; i < 4; i++) {
    const uint8_t neg_axis = joystick_axes[i][0];
    const uint8_t pos_axis = joystick_axes[i][1];
    // Scale range from [0, 255] to [0, 32767]
    const int16_t joystick_state = joystick_states[i] << 7;

    // Assign signed joystick values to the report
    if (ANALOG_STATE(pos_axis) > ANALOG_STATE(neg_axis))
      // Positive axis
      report.joysticks[i] = joystick_state;
    else
      // Negative axis
      report.joysticks[i] = -joystick_state;
  }

#if defined(JOYSTICK_ENABLED)
  joystick_state_t j_state = joystick_get_state();
  joystick_config_t j_config = joystick_get_config();
  
  // XInput axes are scaled to -32768..32767. Our joystick outputs -128..127.
  // Use proportional scaling so positive max (127) maps to 32767 and
  // negative min (-128) maps to -32768.
  if (j_config.mode == JOYSTICK_MODE_XINPUT_LS) {
      report.joysticks[0] = (j_state.out_x > 0)
          ? (int16_t)((int32_t)j_state.out_x * 32767 / 127)
          : (int16_t)((int32_t)j_state.out_x * 32768 / 128);
      // Depending on hardware, Y axis might be inverted. Usually up is positive in XInput.
      report.joysticks[1] = (j_state.out_y > 0)
          ? -(int16_t)((int32_t)j_state.out_y * 32767 / 127)
          : -(int16_t)((int32_t)j_state.out_y * 32768 / 128);
      if (j_state.sw) report.buttons |= XINPUT_BUTTON_LS;
  } else if (j_config.mode == JOYSTICK_MODE_XINPUT_RS) {
      report.joysticks[2] = (j_state.out_x > 0)
          ? (int16_t)((int32_t)j_state.out_x * 32767 / 127)
          : (int16_t)((int32_t)j_state.out_x * 32768 / 128);
      report.joysticks[3] = (j_state.out_y > 0)
          ? -(int16_t)((int32_t)j_state.out_y * 32767 / 127)
          : -(int16_t)((int32_t)j_state.out_y * 32768 / 128);
      if (j_state.sw) report.buttons |= XINPUT_BUTTON_RS;
  }
#endif

  if (tud_ready() && endpoint_in != 0 && !usbd_edpt_busy(0, endpoint_in) &&
      // Only send report if it has changed
      memcmp(&report, &last_report, sizeof(xinput_report_t)) != 0) {
    usbd_edpt_claim(0, endpoint_in);
    usbd_edpt_xfer(0, endpoint_in, (uint8_t *)&report, sizeof(xinput_report_t));
    usbd_edpt_release(0, endpoint_in);
    // Update the last report to the current one
    memcpy(&last_report, &report, sizeof(xinput_report_t));
  }

  // Reset analog states for the next scan
  memset(analog_states, 0, sizeof(analog_states));

  // ---------------------------------------------------------------
  // HID Gamepad Report (Xbox-compatible)
  // ---------------------------------------------------------------
  // Only send HID gamepad report when XInput is disabled.
  // On Windows, XInput is used natively; sending both XInput and HID
  // gamepad reports would cause dual gamepad recognition.
  // On Linux/macOS where XInput is unavailable, the HID gamepad
  // provides native gamepad support.
  if (!eeconfig->options.xinput_enabled) {
    static hid_gamepad_xbox_report_t gp_report;
    static hid_gamepad_xbox_report_t prev_gp_report;

    // Joystick axes (same values as XInput report)
    gp_report.lx = report.joysticks[0];
    gp_report.ly = report.joysticks[1];
    gp_report.rx = report.joysticks[2];
    gp_report.ry = report.joysticks[3];

    // Triggers (same values as XInput report)
    gp_report.lt = report.lz;
    gp_report.rt = report.rz;

    // Hat switch: convert D-pad buttons to 8-direction hat
    {
      const bool up = report.buttons & XINPUT_BUTTON_UP;
      const bool down = report.buttons & XINPUT_BUTTON_DOWN;
      const bool left = report.buttons & XINPUT_BUTTON_LEFT;
      const bool right = report.buttons & XINPUT_BUTTON_RIGHT;

      if (up && right)
        gp_report.hat = 2;
      else if (up && left)
        gp_report.hat = 8;
      else if (down && right)
        gp_report.hat = 4;
      else if (down && left)
        gp_report.hat = 6;
      else if (up)
        gp_report.hat = 1;
      else if (right)
        gp_report.hat = 3;
      else if (down)
        gp_report.hat = 5;
      else if (left)
        gp_report.hat = 7;
      else
        gp_report.hat = 0; // Neutral (null state)
    }

    // Button mapping: XInput → HID buttons 1-16
    // HID Button 1=A, 2=B, 3=X, 4=Y, 5=LB, 6=RB, 7=Back, 8=Start,
    //            9=LS, 10=RS, 11=Home
    {
      uint16_t hid_buttons = 0;
      if (report.buttons & XINPUT_BUTTON_A)     hid_buttons |= (1 << 0);
      if (report.buttons & XINPUT_BUTTON_B)     hid_buttons |= (1 << 1);
      if (report.buttons & XINPUT_BUTTON_X)     hid_buttons |= (1 << 2);
      if (report.buttons & XINPUT_BUTTON_Y)     hid_buttons |= (1 << 3);
      if (report.buttons & XINPUT_BUTTON_LB)    hid_buttons |= (1 << 4);
      if (report.buttons & XINPUT_BUTTON_RB)    hid_buttons |= (1 << 5);
      if (report.buttons & XINPUT_BUTTON_BACK)  hid_buttons |= (1 << 6);
      if (report.buttons & XINPUT_BUTTON_START) hid_buttons |= (1 << 7);
      if (report.buttons & XINPUT_BUTTON_LS)    hid_buttons |= (1 << 8);
      if (report.buttons & XINPUT_BUTTON_RS)    hid_buttons |= (1 << 9);
      if (report.buttons & XINPUT_BUTTON_HOME)  hid_buttons |= (1 << 10);
      gp_report.buttons = hid_buttons;
    }

    // Send HID gamepad report if it changed
    if (tud_hid_n_ready(USB_ITF_HID) &&
        memcmp(&gp_report, &prev_gp_report, sizeof(gp_report)) != 0) {
      tud_hid_n_report(USB_ITF_HID, REPORT_ID_GAMEPAD, &gp_report,
                       sizeof(gp_report));
      memcpy(&prev_gp_report, &gp_report, sizeof(gp_report));
    }
  }
}

//---------------------------------------------------------------------+
// TinyUSB Callbacks
//---------------------------------------------------------------------+

static void xinput_driver_init(void) {}

static void xinput_driver_reset(uint8_t rhport) {}

static uint16_t xinput_driver_open(uint8_t rhport,
                                   const tusb_desc_interface_t *desc_intf,
                                   uint16_t max_len) {
  if (desc_intf->bInterfaceClass == TUSB_CLASS_VENDOR_SPECIFIC &&
      desc_intf->bInterfaceSubClass == XINPUT_SUBCLASS_DEFAULT &&
      desc_intf->bInterfaceProtocol == XINPUT_PROTOCOL_DEFAULT) {
    TU_ASSERT(usbd_open_edpt_pair(rhport, tu_desc_next(tu_desc_next(desc_intf)),
                                  desc_intf->bNumEndpoints, TUSB_XFER_INTERRUPT,
                                  &endpoint_out, &endpoint_in),
              0);

    return XINPUT_DESC_LEN;
  }

  return 0;
}

static bool
xinput_driver_control_xfer_cb(uint8_t rhport, uint8_t stage,
                              tusb_control_request_t const *request) {
  return true;
}

static bool xinput_driver_xfer_cb(uint8_t rhport, uint8_t ep_addr,
                                  xfer_result_t result,
                                  uint32_t xferred_bytes) {
  return true;
}

static const usbd_class_driver_t xinput_driver = {
    .init = xinput_driver_init,
    .reset = xinput_driver_reset,
    .open = xinput_driver_open,
    .control_xfer_cb = xinput_driver_control_xfer_cb,
    .xfer_cb = xinput_driver_xfer_cb,
    .sof = NULL,
};

const usbd_class_driver_t *usbd_app_driver_get_cb(uint8_t *driver_count) {
  *driver_count = 1;

  return &xinput_driver;
}
