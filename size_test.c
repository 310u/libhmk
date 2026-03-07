#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define NUM_LAYERS 4
#define NUM_KEYS 40
#define NUM_ADVANCED_KEYS 32
#define NUM_MACROS 16
#define RGB_ENABLED
#define JOYSTICK_ENABLED
#define DEFAULT_CALIBRATION 0
#define DEFAULT_KEYMAPS 0
#define FLASH_SIZE 256000
#define WL_VIRTUAL_SIZE 4096
#define WL_WRITE_LOG_SIZE 4096

#define NUM_PROFILES 4

#include "include/common.h"
#include "include/rgb.h"
#include "include/joystick.h"
#include "include/eeconfig.h"

int main() {
    printf("Global config size expected: %d\n", 16 + NUM_KEYS * 2);
    printf("Global config size actual: %zu\n", sizeof(eeconfig_t) - sizeof(eeconfig_profile_t)*4 - 4);
    
    printf("Profile size expected: %d\n", NUM_LAYERS * NUM_KEYS + NUM_KEYS * 4 + NUM_ADVANCED_KEYS * 13 + NUM_KEYS + 9 + 1 + NUM_MACROS * sizeof(macro_t) + 7 + 1 + 2 + 3 * NUM_LAYERS + 3 * NUM_KEYS + 20);
    printf("Profile size actual: %zu\n", sizeof(eeconfig_profile_t));
    return 0;
}
