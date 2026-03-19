#pragma once

#include <stdbool.h>
#include <stdint.h>

#define TUSB_CLASS_VENDOR_SPECIFIC 0xFF
#define TUSB_XFER_INTERRUPT 0x03

typedef uint8_t xfer_result_t;
typedef struct {
  uint8_t dummy;
} tusb_control_request_t;

typedef struct __attribute__((packed)) {
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bInterfaceNumber;
  uint8_t bAlternateSetting;
  uint8_t bNumEndpoints;
  uint8_t bInterfaceClass;
  uint8_t bInterfaceSubClass;
  uint8_t bInterfaceProtocol;
  uint8_t iInterface;
} tusb_desc_interface_t;

typedef struct {
  void (*init)(void);
  void (*reset)(uint8_t rhport);
  uint16_t (*open)(uint8_t rhport, const tusb_desc_interface_t *desc_intf,
                   uint16_t max_len);
  bool (*control_xfer_cb)(uint8_t rhport, uint8_t stage,
                          tusb_control_request_t const *request);
  bool (*xfer_cb)(uint8_t rhport, uint8_t ep_addr, xfer_result_t result,
                  uint32_t xferred_bytes);
  void *sof;
} usbd_class_driver_t;

static inline const void *tu_desc_next(const void *desc) { return desc; }

#define TU_ASSERT(condition, ret)                                               \
  do {                                                                          \
    if (!(condition))                                                           \
      return (ret);                                                             \
  } while (0)

bool tud_ready(void);
bool tud_hid_n_ready(uint8_t instance);
bool tud_hid_n_report(uint8_t instance, uint8_t report_id, const void *report,
                      uint16_t len);
