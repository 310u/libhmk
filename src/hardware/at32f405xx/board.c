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

#include "at32f402_405.h"
#include "tusb.h"

// Keyboard board definitions may override these when a design needs a
// clock tree different from the driver default.
#if !defined(BOARD_FLASH_WAIT_CYCLES)
#define BOARD_FLASH_WAIT_CYCLES FLASH_WAIT_CYCLE_6
#endif

#if !defined(BOARD_PWC_LDO_OUTPUT)
#define BOARD_PWC_LDO_OUTPUT PWC_LDO_OUTPUT_1V3
#endif

#if !defined(BOARD_PLL_NS)
#define BOARD_PLL_NS 72u
#endif

#if !defined(BOARD_PLL_MS)
#define BOARD_PLL_MS 1u
#endif

#if !defined(BOARD_PLL_FP)
#define BOARD_PLL_FP CRM_PLL_FP_4
#endif

#if !defined(BOARD_PLL_FU)
#define BOARD_PLL_FU CRM_PLL_FU_18
#endif

#if !defined(BOARD_AHB_DIV)
#define BOARD_AHB_DIV CRM_AHB_DIV_1
#endif

#if !defined(BOARD_APB2_DIV)
#define BOARD_APB2_DIV CRM_APB2_DIV_1
#endif

#if !defined(BOARD_APB1_DIV)
#define BOARD_APB1_DIV CRM_APB1_DIV_2
#endif

/**
 * @brief Initialize the clock
 *
 * @return None
 */
static void board_clock_init(void) {
  // Reset the CRM
  crm_reset();
  // Configure flash PSR register for the selected system clock
  flash_psr_set(BOARD_FLASH_WAIT_CYCLES);
  // Enable PWC peripheral clock
  crm_periph_clock_enable(CRM_PWC_PERIPH_CLOCK, TRUE);
  // Set power LDO output voltage for the selected system clock
  pwc_ldo_output_voltage_set(BOARD_PWC_LDO_OUTPUT);
  // Set clock source to HSE
  crm_clock_source_enable(CRM_CLOCK_SOURCE_HEXT, TRUE);

  // Wait for HSE to stabilize
  while (crm_hext_stable_wait() == ERROR)
    ;

  // Configure PLL
  crm_pll_config(CRM_PLL_SOURCE_HEXT, BOARD_PLL_NS, BOARD_PLL_MS, BOARD_PLL_FP);
  // Configure PLLU to 48MHz for USB FS
  crm_pllu_div_set(BOARD_PLL_FU);
  // Enable PLL as system clock source
  crm_clock_source_enable(CRM_CLOCK_SOURCE_PLL, TRUE);

  // Wait for PLL to stabilize
  while (crm_flag_get(CRM_PLL_STABLE_FLAG) != SET)
    ;

  // Configure AHB clock
  crm_ahb_div_set(BOARD_AHB_DIV);
  // Configure APB2 clock
  crm_apb2_div_set(BOARD_APB2_DIV);
  // Configure APB1 clock
  crm_apb1_div_set(BOARD_APB1_DIV);
  // Enable auto step mode before switching system clock source to PLL
  crm_auto_step_mode_enable(TRUE);
  // Select PLL as system clock source
  crm_sysclk_switch(CRM_SCLK_PLL);

  // Wait for system clock to finish switching
  while (crm_sysclk_switch_status_get() != CRM_SCLK_PLL)
    ;

  // Disable auto step mode after switching system clock source
  crm_auto_step_mode_enable(FALSE);
  // Update system core clock variable
  system_core_clock_update();
}

static void board_usb_ignore_vbus(void) {
  // Match the AT32 TinyUSB BSP and force both OTG blocks to ignore VBUS.
  OTG1_GLOBAL->gccfg_bit.vbusig = TRUE;
  OTG2_GLOBAL->gccfg_bit.vbusig = TRUE;
}

/**
 * @brief Initialize the USB
 *
 * @return None
 */
static void board_usb_init(void) {
#if defined(BOARD_USB_FS)
  crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
#elif defined(BOARD_USB_HS)
  crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
#endif

  // Keep both OTG blocks clocked so VBUS override and wakeup handling match
  // the known-good TinyUSB AT32 BSP behavior.
  crm_periph_clock_enable(CRM_OTGFS1_PERIPH_CLOCK, TRUE);
  crm_periph_clock_enable(CRM_OTGHS_PERIPH_CLOCK, TRUE);

  // Configure PLLU for USB
  crm_pllu_output_set(TRUE);

  // Wait for PLLU output to stabilize
  while (crm_flag_get(CRM_PLLU_STABLE_FLAG) != SET)
    ;

  // Configure USB clock source to PLLU
  crm_usb_clock_source_select(CRM_USB_CLOCK_SOURCE_PLLU);

  board_usb_ignore_vbus();
#if defined(BOARD_USB_FS)
  // Set NVIC priority for USB FS interrupt
  NVIC_SetPriority(OTGFS1_IRQn, 0);
  NVIC_SetPriority(OTGFS1_WKUP_IRQn, 0);
  NVIC_EnableIRQ(OTGFS1_WKUP_IRQn);
#elif defined(BOARD_USB_HS)
  // Set NVIC priority for USB HS interrupt
  NVIC_SetPriority(OTGHS_IRQn, 0);
  NVIC_SetPriority(OTGHS_WKUP_IRQn, 0);
  NVIC_EnableIRQ(OTGHS_WKUP_IRQn);
#endif
}

static void board_bootloader_jump(void) {
  volatile const uint32_t *bootloader_vector =
      (volatile const uint32_t *)BOOTLOADER_ADDR;
  uint32_t sp = bootloader_vector[0];
  uint32_t bootloader_entry = bootloader_vector[1];

  SysTick->CTRL = 0;
  SysTick->LOAD = 0;
  SysTick->VAL = 0;

  // Disable all interrupts
  NVIC->ICER[0] = 0xFFFFFFFF;
  NVIC->ICER[1] = 0xFFFFFFFF;
  NVIC->ICER[2] = 0xFFFFFFFF;
  NVIC->ICER[3] = 0xFFFFFFFF;

  SCB->VTOR = (uint32_t)BOOTLOADER_ADDR;

  // Set stack pointer
  __set_MSP(sp);
  __set_PSP(sp);

  ((void (*)(void))bootloader_entry)();
  while (1)
    ;
}

extern uint32_t _board_bootloader_flag[];
#define BOARD_BOOTLOADER_FLAG _board_bootloader_flag[0]

void board_init(void) {
  // We need to enter the bootloader before the clock peripheral is initialized
  // since it is initialized differently in the bootloader.
  if (BOARD_BOOTLOADER_FLAG == BOOTLOADER_MAGIC) {
    // Clear the bootloader flag before jumping to the bootloader
    BOARD_BOOTLOADER_FLAG = 0;
    board_bootloader_jump();
  }

  // Configure NVIC priority group
  nvic_priority_group_config(NVIC_PRIORITY_GROUP_4);

  board_clock_init();
  board_usb_init();

  // Enable cycle counter
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  DWT->CYCCNT = 0;
}

void board_error_handler(void) {
  __disable_irq();
  while (1)
    ;
}

void board_reset(void) { NVIC_SystemReset(); }

void board_enter_bootloader(void) {
  // Set the bootloader flag
  BOARD_BOOTLOADER_FLAG = BOOTLOADER_MAGIC;

  tud_disconnect();

  uint32_t start_cycles = board_cycle_count();
  // 10ms delay (system_core_clock is cycles per second)
  uint32_t delay_cycles = system_core_clock / 100;
  while (board_cycle_count() - start_cycles < delay_cycles)
    ;

  // Reset the board to enter the bootloader
  NVIC_SystemReset();
}

uint32_t board_serial(char *buf) {
  const volatile uint8_t *uid = (const volatile uint8_t *)(0x1FFFF7E8);
  // Use the 96-bit unique ID as the serial number
  for (uint32_t i = 0; i < 12; i++) {
    buf[i * 2] = M_HEX(uid[i] >> 4);
    buf[i * 2 + 1] = M_HEX(uid[i] & 0x0F);
  }

  return 24;
}

uint32_t board_cycle_count(void) { return DWT->CYCCNT; }

//--------------------------------------------------------------------+
// Interrupt Handlers
//--------------------------------------------------------------------+

// AT32F405 has a weird behavior, where if it has an unstable D+/D- during
// startup, there will be a pending suspend interrupt before enumeration is
// complete. This caused TinyUSB internal state to get stuck in a suspend state.
// The issue was fixed in https://github.com/hathach/tinyusb/pull/3319.

void OTGFS1_IRQHandler(void) { tud_int_handler(0); }

void OTGHS_IRQHandler(void) { tud_int_handler(1); }

void OTGFS1_WKUP_IRQHandler(void) { tud_int_handler(0); }

void OTGHS_WKUP_IRQHandler(void) { tud_int_handler(1); }
