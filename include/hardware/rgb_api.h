#pragma once

#include "common.h"

#if defined(RGB_ENABLED)

void rgb_driver_init(void);
void rgb_driver_task(void);
void rgb_driver_write(const uint8_t *grb_data, uint16_t byte_count);

#endif
