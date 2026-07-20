#include <unity.h>

#include "hal/logic/debounce.h"
#include "hal/logic/dip_switch_decode.h"
#include "hal/logic/ultrasonic_math.h"
#include "hal/logic/ultrasonic_schedule.h"

using hal::logic::debounce_update;
using hal::logic::DebounceState;
using hal::logic::dip_switch_decode;
using hal::logic::ultrasonic_duration_us_to_cm;
using hal::logic::UltrasonicScheduler;

void setUp() {}
void tearDown() {}

void test_debounce_ignores_short_glitch() {
  DebounceState state;
  TEST_ASSERT_FALSE(debounce_update(state, false, 0, 20));
  TEST_ASSERT_FALSE(debounce_update(state, true, 5, 20));    // glitch, window not elapsed yet
  TEST_ASSERT_FALSE(debounce_update(state, false, 10, 20));  // back to false before debounce elapsed
}

void test_debounce_accepts_stable_change() {
  DebounceState state;
  debounce_update(state, false, 0, 20);
  debounce_update(state, true, 5, 20);
  TEST_ASSERT_TRUE(debounce_update(state, true, 30, 20));  // held true past the debounce window
}

void test_ultrasonic_conversion_known_value() {
  // 343 m/s -> 0.0343 cm/us round trip -> 1000us should be ~17.15 cm one-way.
  float cm = ultrasonic_duration_us_to_cm(1000);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 17.15f, cm);
}

void test_dip_switch_decode_bits() {
  bool bits[5] = {true, false, true, false, false};
  TEST_ASSERT_EQUAL_UINT8(0b00101, dip_switch_decode(bits));
}

void test_dip_switch_decode_all_off() {
  bool bits[5] = {false, false, false, false, false};
  TEST_ASSERT_EQUAL_UINT8(0, dip_switch_decode(bits));
}

void test_ultrasonic_scheduler_baseline_before_any_stimulus() {
  UltrasonicScheduler s;
  TEST_ASSERT_FALSE(s.isActive(0));
  TEST_ASSERT_EQUAL_UINT32(UltrasonicScheduler::kBaselinePeriodMs, s.periodMs(123456));
}

void test_ultrasonic_scheduler_active_window_and_expiry() {
  UltrasonicScheduler s;
  s.onStimulus(1000);
  TEST_ASSERT_EQUAL_UINT32(UltrasonicScheduler::kActivePeriodMs, s.periodMs(1000));
  TEST_ASSERT_EQUAL_UINT32(UltrasonicScheduler::kActivePeriodMs, s.periodMs(60999));
  TEST_ASSERT_EQUAL_UINT32(UltrasonicScheduler::kBaselinePeriodMs, s.periodMs(61000));
}

void test_ultrasonic_scheduler_stimulus_renews_window() {
  UltrasonicScheduler s;
  s.onStimulus(1000);
  s.onStimulus(50000);  // renewed before expiry
  TEST_ASSERT_TRUE(s.isActive(100000));
  TEST_ASSERT_FALSE(s.isActive(110000));
}

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;

  UNITY_BEGIN();
  RUN_TEST(test_debounce_ignores_short_glitch);
  RUN_TEST(test_debounce_accepts_stable_change);
  RUN_TEST(test_ultrasonic_conversion_known_value);
  RUN_TEST(test_dip_switch_decode_bits);
  RUN_TEST(test_dip_switch_decode_all_off);
  RUN_TEST(test_ultrasonic_scheduler_baseline_before_any_stimulus);
  RUN_TEST(test_ultrasonic_scheduler_active_window_and_expiry);
  RUN_TEST(test_ultrasonic_scheduler_stimulus_renews_window);
  return UNITY_END();
}
