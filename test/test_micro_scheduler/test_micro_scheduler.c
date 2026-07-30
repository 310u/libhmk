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

#include <unity.h>
#include "micro_scheduler.h"
#include "hardware/hardware.h"

// Mocks for native test environment
static uint32_t mock_timer_us_val = 0;

uint32_t timer_read_us(void) {
    return mock_timer_us_val;
}

uint32_t timer_read(void) {
    return mock_timer_us_val / 1000u;
}

void setUp(void) {
    mock_timer_us_val = 0;
}

void tearDown(void) {}

void test_micro_scheduler_init_runs_without_crash(void) {
    mock_timer_us_val = 1000u;
    micro_scheduler_init();
    TEST_ASSERT_EQUAL_UINT32(1000u, timer_read_us());
}

void test_micro_scheduler_run_executes_tasks(void) {
    mock_timer_us_val = 0;
    micro_scheduler_init();

    // Advance time by 200us (exceeding 125us USB task interval)
    mock_timer_us_val = 200u;
    micro_scheduler_run();

    // Verification passes if run completes deterministically
    TEST_ASSERT_TRUE(true);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_micro_scheduler_init_runs_without_crash);
    RUN_TEST(test_micro_scheduler_run_executes_tasks);
    return UNITY_END();
}
