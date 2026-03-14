#pragma once

// Minimal build-time constants for native unit tests.
#define NUM_KEYS 10
#define NUM_LAYERS 4
#define NUM_PROFILES 3
#define NUM_ADVANCED_KEYS 16

#define WL_VIRTUAL_SIZE 8192
#define WL_WRITE_LOG_SIZE 1024
#define FLASH_SIZE 65536

#define F_CPU 216000000
#define ADC_RESOLUTION 12
#define ADC_NUM_CHANNELS 1
#define ADC_NUM_RAW_INPUTS 1
#define ADC_RAW_INPUT_CHANNELS {0}
#define ADC_RAW_INPUT_VECTOR {0}

#define DEFAULT_CALIBRATION {0}
#define DEFAULT_KEYMAPS {{{0}}}
