#include <unity.h>

#include "hal/logic/debounce.h"
#include "hal/logic/dip_switch_decode.h"
#include "hal/logic/persist_image.h"
#include "hal/logic/ultrasonic_math.h"
#include "hal/logic/ultrasonic_schedule.h"

using hal::logic::debounce_update;
using hal::logic::DebounceState;
using hal::logic::dip_switch_decode;
using hal::logic::persist_build_header;
using hal::logic::persist_header_valid;
using hal::logic::persist_select_slots;
using hal::logic::persist_sequence_newer;
using hal::logic::PersistHeader;
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

void test_persist_header_roundtrip_valid() {
  uint8_t payload[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  PersistHeader header = persist_build_header(sizeof(payload), 42, payload);
  TEST_ASSERT_TRUE(persist_header_valid(header, payload, sizeof(payload)));
}

void test_persist_header_rejects_corrupt_payload() {
  uint8_t payload[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  PersistHeader header = persist_build_header(sizeof(payload), 42, payload);
  payload[3] ^= 0xFF;  // bit-flip after the header was sealed
  TEST_ASSERT_FALSE(persist_header_valid(header, payload, sizeof(payload)));
}

void test_persist_header_rejects_bad_magic() {
  PersistHeader header{};  // zero-initialized -- magic=0, never a valid image
  uint8_t payload[4] = {0, 0, 0, 0};
  TEST_ASSERT_FALSE(persist_header_valid(header, payload, sizeof(payload)));
}

void test_persist_header_rejects_oversized_payload_len() {
  uint8_t payload[4] = {1, 2, 3, 4};
  PersistHeader header = persist_build_header(sizeof(payload), 1, payload);
  header.payloadLen = 200;  // claims more than the caller's buffer holds
  TEST_ASSERT_FALSE(persist_header_valid(header, payload, sizeof(payload)));
}

void test_persist_select_slots_first_boot_neither_valid() {
  auto decision = persist_select_slots(false, 0, false, 0);
  TEST_ASSERT_EQUAL_INT(-1, decision.loadSlot);
  TEST_ASSERT_EQUAL_UINT8(0, decision.writeSlot);
  TEST_ASSERT_EQUAL_UINT32(1, decision.nextSequence);
}

void test_persist_select_slots_only_slot0_valid() {
  auto decision = persist_select_slots(true, 5, false, 0);
  TEST_ASSERT_EQUAL_INT(0, decision.loadSlot);
  TEST_ASSERT_EQUAL_UINT8(1, decision.writeSlot);
  TEST_ASSERT_EQUAL_UINT32(6, decision.nextSequence);
}

void test_persist_select_slots_prefers_newer_sequence() {
  auto decision = persist_select_slots(true, 5, true, 9);
  TEST_ASSERT_EQUAL_INT(1, decision.loadSlot);
  TEST_ASSERT_EQUAL_UINT8(0, decision.writeSlot);
  TEST_ASSERT_EQUAL_UINT32(10, decision.nextSequence);
}

void test_persist_sequence_newer_handles_wraparound() {
  TEST_ASSERT_TRUE(persist_sequence_newer(1, 0xFFFFFFFFu));
  TEST_ASSERT_FALSE(persist_sequence_newer(0xFFFFFFFFu, 1));
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
  RUN_TEST(test_persist_header_roundtrip_valid);
  RUN_TEST(test_persist_header_rejects_corrupt_payload);
  RUN_TEST(test_persist_header_rejects_bad_magic);
  RUN_TEST(test_persist_header_rejects_oversized_payload_len);
  RUN_TEST(test_persist_select_slots_first_boot_neither_valid);
  RUN_TEST(test_persist_select_slots_only_slot0_valid);
  RUN_TEST(test_persist_select_slots_prefers_newer_sequence);
  RUN_TEST(test_persist_sequence_newer_handles_wraparound);
  return UNITY_END();
}
