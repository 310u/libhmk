#pragma once

#define NUM_KEYS 41
#define NUM_LAYERS 4
#define NUM_PROFILES 4
#define NUM_ADVANCED_KEYS 32

#define WL_VIRTUAL_SIZE 8192
#define WL_WRITE_LOG_SIZE 1024
#define FLASH_SIZE 65536

#if !defined(F_CPU)
#define F_CPU 216000000
#endif

#if !defined(ADC_RESOLUTION)
#define ADC_RESOLUTION 12
#endif

#if !defined(ADC_NUM_CHANNELS)
#define ADC_NUM_CHANNELS 1
#endif

#if !defined(ADC_NUM_RAW_INPUTS)
#define ADC_NUM_RAW_INPUTS 1
#endif

#if !defined(ADC_RAW_INPUT_CHANNELS)
#define ADC_RAW_INPUT_CHANNELS {0}
#endif

#if !defined(ADC_RAW_INPUT_VECTOR)
#define ADC_RAW_INPUT_VECTOR {0}
#endif

#define DEFAULT_CALIBRATION {0}
#define DEFAULT_KEYMAPS {{{0}}}
