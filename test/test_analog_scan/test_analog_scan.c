#include <unity.h>

#include "analog_scan.h"

void setUp(void) { analog_scan_reset(); }

void tearDown(void) {}

void test_analog_scan_stores_mux_and_raw_samples(void) {
  static const uint16_t mux0_samples[] = {111, 222, 333, 444};
  static const uint16_t mux1_samples[] = {555, 666, 777, 888};

  analog_scan_store_samples(mux0_samples, 0);
  analog_scan_store_samples(mux1_samples, 1);

  TEST_ASSERT_EQUAL_UINT16(111, analog_scan_read_key(0));
  TEST_ASSERT_EQUAL_UINT16(555, analog_scan_read_key(1));
  TEST_ASSERT_EQUAL_UINT16(222, analog_scan_read_key(2));
  TEST_ASSERT_EQUAL_UINT16(777, analog_scan_read_key(3));

  TEST_ASSERT_EQUAL_UINT16(777, analog_scan_read_raw(0));
  TEST_ASSERT_EQUAL_UINT16(888, analog_scan_read_raw(1));
}

void test_analog_scan_reset_clears_key_and_raw_values(void) {
  static const uint16_t samples[] = {10, 20, 30, 40};

  analog_scan_store_samples(samples, 0);
  analog_scan_reset();

  TEST_ASSERT_EQUAL_UINT16(0, analog_scan_read_key(0));
  TEST_ASSERT_EQUAL_UINT16(0, analog_scan_read_key(2));
  TEST_ASSERT_EQUAL_UINT16(0, analog_scan_read_raw(0));
  TEST_ASSERT_EQUAL_UINT16(0, analog_scan_read_raw(1));
}

void test_analog_scan_out_of_range_reads_return_zero(void) {
  static const uint16_t samples[] = {10, 20, 30, 40};

  analog_scan_store_samples(samples, 0);

  TEST_ASSERT_EQUAL_UINT16(0, analog_scan_read_key(NUM_KEYS));
  TEST_ASSERT_EQUAL_UINT16(0, analog_scan_read_raw(ADC_NUM_RAW_INPUTS));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_analog_scan_stores_mux_and_raw_samples);
  RUN_TEST(test_analog_scan_reset_clears_key_and_raw_values);
  RUN_TEST(test_analog_scan_out_of_range_reads_return_zero);
  return UNITY_END();
}
