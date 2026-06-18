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

void test_analog_scan_generation_is_consumed_once_per_recorded_full_scan(void) {
  uint32_t last_seen_generation = 0u;
  uint32_t missed_count = 0u;

  TEST_ASSERT_FALSE(
      analog_scan_consume_full_scan_generation(&last_seen_generation,
                                               &missed_count));
  TEST_ASSERT_EQUAL_UINT32(0u, analog_scan_get_full_scan_generation());

  analog_scan_record_full_scan_generation();

  TEST_ASSERT_TRUE(
      analog_scan_consume_full_scan_generation(&last_seen_generation,
                                               &missed_count));
  TEST_ASSERT_EQUAL_UINT32(1u, last_seen_generation);
  TEST_ASSERT_EQUAL_UINT32(0u, missed_count);
  TEST_ASSERT_FALSE(
      analog_scan_consume_full_scan_generation(&last_seen_generation,
                                               &missed_count));
}

void test_analog_scan_generation_consume_counts_missed_full_scans(void) {
  uint32_t last_seen_generation = 1u;
  uint32_t missed_count = 0u;

  analog_scan_record_full_scan_generation();
  analog_scan_record_full_scan_generation();
  analog_scan_record_full_scan_generation();

  TEST_ASSERT_TRUE(
      analog_scan_consume_full_scan_generation(&last_seen_generation,
                                               &missed_count));
  TEST_ASSERT_EQUAL_UINT32(3u, last_seen_generation);
  TEST_ASSERT_EQUAL_UINT32(1u, missed_count);
}

void test_analog_scan_reset_clears_full_scan_generation(void) {
  analog_scan_record_full_scan_generation();
  TEST_ASSERT_EQUAL_UINT32(1u, analog_scan_get_full_scan_generation());

  analog_scan_reset();

  TEST_ASSERT_EQUAL_UINT32(0u, analog_scan_get_full_scan_generation());
}

void test_analog_diag_capture_baseline_and_raw_by_step_snapshot(void) {
  static const uint16_t mux0_samples[] = {111, 222, 333, 444};
  static const uint16_t mux1_samples[] = {555, 666, 777, 888};

  analog_scan_store_samples(mux0_samples, 0);
  analog_scan_store_samples(mux1_samples, 1);
  analog_diag_capture_baseline();

  TEST_ASSERT_TRUE(analog_diag_channel_identity_enabled());
  TEST_ASSERT_EQUAL_UINT8(2u, analog_diag_mux_step_count());
  TEST_ASSERT_EQUAL_UINT8(2u, analog_diag_adc_lane_count());
  TEST_ASSERT_EQUAL_UINT16(111u, analog_diag_read_raw_by_step(0u, 0u));
  TEST_ASSERT_EQUAL_UINT16(222u, analog_diag_read_baseline_by_step(0u, 1u));
  TEST_ASSERT_EQUAL_UINT16(666u, analog_diag_read_baseline_by_step(1u, 1u));
}

void test_analog_diag_channel_identity_passes_for_expected_key(void) {
  static const uint16_t baseline_step0[] = {1000, 2000, 3000, 4000};
  static const uint16_t baseline_step1[] = {1100, 2100, 3100, 4100};
  static const uint16_t active_step0[] = {1180, 2005, 3000, 4000};
  static const uint16_t active_step1[] = {1100, 2100, 3100, 4100};
  analog_channel_identity_result_t result;

  analog_scan_store_samples(baseline_step0, 0u);
  analog_scan_store_samples(baseline_step1, 1u);
  analog_diag_capture_baseline();
  analog_scan_store_samples(active_step0, 0u);
  analog_scan_store_samples(active_step1, 1u);

  TEST_ASSERT_TRUE(
      analog_diag_run_channel_identity_test(0u, 64u, 20u, &result));
  TEST_ASSERT_TRUE(result.pass);
  TEST_ASSERT_EQUAL_UINT8(0u, result.expected_step);
  TEST_ASSERT_EQUAL_UINT8(0u, result.expected_lane);
  TEST_ASSERT_EQUAL_UINT8(0u, result.observed_max_step);
  TEST_ASSERT_EQUAL_UINT8(0u, result.observed_max_lane);
  TEST_ASSERT_EQUAL_UINT8(0u, result.observed_logical_key);
  TEST_ASSERT_EQUAL_UINT16(180u, result.observed_max_delta);
  TEST_ASSERT_EQUAL_UINT16(5u, analog_diag_read_delta_by_step(0u, 1u));
}

void test_analog_diag_channel_identity_flags_one_step_offset(void) {
  static const uint16_t baseline_step0[] = {1000, 2000, 3000, 4000};
  static const uint16_t baseline_step1[] = {1100, 2100, 3100, 4100};
  static const uint16_t active_step0[] = {1000, 2000, 3000, 4000};
  static const uint16_t active_step1[] = {1260, 2100, 3100, 4100};
  analog_channel_identity_result_t result;

  analog_scan_store_samples(baseline_step0, 0u);
  analog_scan_store_samples(baseline_step1, 1u);
  analog_diag_capture_baseline();
  analog_scan_store_samples(active_step0, 0u);
  analog_scan_store_samples(active_step1, 1u);

  TEST_ASSERT_TRUE(
      analog_diag_run_channel_identity_test(0u, 64u, 20u, &result));
  TEST_ASSERT_FALSE(result.pass);
  TEST_ASSERT_EQUAL_UINT8(1u, result.observed_max_step);
  TEST_ASSERT_EQUAL_UINT8(0u, result.observed_max_lane);
  TEST_ASSERT_EQUAL_UINT8(1u, result.observed_logical_key);
  TEST_ASSERT_EQUAL_UINT8(ANALOG_CHANNEL_IDENTITY_FAILURE_ONE_STEP_OFFSET,
                          result.failure_reason);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_analog_scan_stores_mux_and_raw_samples);
  RUN_TEST(test_analog_scan_reset_clears_key_and_raw_values);
  RUN_TEST(test_analog_scan_out_of_range_reads_return_zero);
  RUN_TEST(test_analog_scan_generation_is_consumed_once_per_recorded_full_scan);
  RUN_TEST(test_analog_scan_generation_consume_counts_missed_full_scans);
  RUN_TEST(test_analog_scan_reset_clears_full_scan_generation);
  RUN_TEST(test_analog_diag_capture_baseline_and_raw_by_step_snapshot);
  RUN_TEST(test_analog_diag_channel_identity_passes_for_expected_key);
  RUN_TEST(test_analog_diag_channel_identity_flags_one_step_offset);
  return UNITY_END();
}
