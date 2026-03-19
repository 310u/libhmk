#pragma once

#include <stdbool.h>
#include <stdint.h>

bool usbd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer,
                    uint16_t total_bytes);
bool usbd_open_edpt_pair(uint8_t rhport, void const *desc_ep, uint8_t ep_count,
                         uint8_t xfer_type, uint8_t *ep_out, uint8_t *ep_in);
bool usbd_edpt_claim(uint8_t rhport, uint8_t ep_addr);
bool usbd_edpt_release(uint8_t rhport, uint8_t ep_addr);
bool usbd_edpt_busy(uint8_t rhport, uint8_t ep_addr);
