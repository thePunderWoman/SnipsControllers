#include <unity.h>
#include <cstring>

#include "complications.h"

void setUp(void) {}
void tearDown(void) {}

// ---- labels ----------------------------------------------------------

void test_source_labels() {
  TEST_ASSERT_EQUAL_STRING("Battery",
                           complicationSourceLabel(ComplicationSource::kBattery));
  TEST_ASSERT_EQUAL_STRING(
      "Left Slot", complicationSourceLabel(ComplicationSource::kLeftSlot));
  TEST_ASSERT_EQUAL_STRING(
      "Right Slot", complicationSourceLabel(ComplicationSource::kRightSlot));
  TEST_ASSERT_EQUAL_STRING("Signal",
                           complicationSourceLabel(ComplicationSource::kSignal));
  TEST_ASSERT_EQUAL_STRING(
      "Droid Name", complicationSourceLabel(ComplicationSource::kDroidName));
  TEST_ASSERT_EQUAL_STRING(
      "Handedness", complicationSourceLabel(ComplicationSource::kHandedness));
  TEST_ASSERT_EQUAL_STRING("Unknown",
                           complicationSourceLabel(ComplicationSource::kCount));
}

// ---- slot assignment ---------------------------------------------------

void test_default_slot_assignments() {
  ComplicationRegistry registry;
  TEST_ASSERT_TRUE(ComplicationSource::kBattery == registry.slotSource(0));
  TEST_ASSERT_TRUE(ComplicationSource::kDroidName == registry.slotSource(1));
  TEST_ASSERT_TRUE(ComplicationSource::kLeftSlot == registry.slotSource(2));
  TEST_ASSERT_TRUE(ComplicationSource::kRightSlot == registry.slotSource(3));
}

void test_set_slot_source() {
  ComplicationRegistry registry;
  registry.setSlotSource(0, ComplicationSource::kSignal);
  TEST_ASSERT_TRUE(ComplicationSource::kSignal == registry.slotSource(0));
}

void test_set_slot_source_out_of_range_is_noop() {
  ComplicationRegistry registry;
  registry.setSlotSource(99, ComplicationSource::kSignal);
  registry.setSlotSource(-1, ComplicationSource::kSignal);
  // Nothing crashed, and in-range slots are untouched.
  TEST_ASSERT_TRUE(ComplicationSource::kBattery == registry.slotSource(0));
}

void test_slot_source_out_of_range_returns_default() {
  ComplicationRegistry registry;
  TEST_ASSERT_TRUE(ComplicationSource::kBattery == registry.slotSource(99));
}

void test_cycle_slot_source_advances_and_wraps() {
  ComplicationRegistry registry;
  registry.setSlotSource(0, ComplicationSource::kBattery);
  registry.cycleSlotSource(0);
  TEST_ASSERT_TRUE(ComplicationSource::kLeftSlot == registry.slotSource(0));

  // Cycle all the way around back to kBattery.
  for (int i = 0; i < static_cast<int>(ComplicationSource::kCount) - 1;
       ++i) {
    registry.cycleSlotSource(0);
  }
  TEST_ASSERT_TRUE(ComplicationSource::kBattery == registry.slotSource(0));
}

void test_cycle_slot_source_out_of_range_is_noop() {
  ComplicationRegistry registry;
  registry.cycleSlotSource(99);  // should not crash
}

// ---- rendering ----------------------------------------------------------

void test_render_battery() {
  ComplicationRegistry registry;
  ComplicationData data;
  data.batteryPercent = 73;
  registry.setData(data);
  registry.setSlotSource(0, ComplicationSource::kBattery);

  ScreenBuffer screen;
  registry.render(&screen);
  TEST_ASSERT_EQUAL_STRING("Battery: 73%", screen.line(0));
}

void test_render_battery_blanks_when_indicator_hidden() {
  ComplicationRegistry registry;
  ComplicationData data;
  data.batteryPercent = 8;
  data.batteryIndicatorVisible = false;
  registry.setData(data);
  registry.setSlotSource(0, ComplicationSource::kBattery);

  ScreenBuffer screen;
  registry.render(&screen);
  TEST_ASSERT_EQUAL_STRING("", screen.line(0));
}

void test_render_left_and_right_slot_with_labels() {
  ComplicationRegistry registry;
  ComplicationData data;
  std::strncpy(data.leftLabel, "Volume", sizeof(data.leftLabel));
  std::strncpy(data.leftValue, "42%", sizeof(data.leftValue));
  std::strncpy(data.rightLabel, "Throttle", sizeof(data.rightLabel));
  std::strncpy(data.rightValue, "88%", sizeof(data.rightValue));
  registry.setData(data);
  registry.setSlotSource(0, ComplicationSource::kLeftSlot);
  registry.setSlotSource(1, ComplicationSource::kRightSlot);

  ScreenBuffer screen;
  registry.render(&screen);
  TEST_ASSERT_EQUAL_STRING("Volume: 42%", screen.line(0));
  TEST_ASSERT_EQUAL_STRING("Throttle: 88%", screen.line(1));
}

void test_render_left_slot_falls_back_when_label_empty() {
  ComplicationRegistry registry;
  registry.setSlotSource(0, ComplicationSource::kLeftSlot);

  ScreenBuffer screen;
  registry.render(&screen);
  TEST_ASSERT_EQUAL_STRING("Left: ", screen.line(0));
}

void test_render_signal_known_and_unknown() {
  ComplicationRegistry registry;
  registry.setSlotSource(0, ComplicationSource::kSignal);

  ScreenBuffer screen;
  registry.render(&screen);
  TEST_ASSERT_EQUAL_STRING("Signal: --", screen.line(0));

  ComplicationData data;
  data.signalKnown = true;
  data.signalDbm = -42;
  registry.setData(data);
  registry.render(&screen);
  TEST_ASSERT_EQUAL_STRING("Signal: -42dBm", screen.line(0));
}

void test_render_droid_name_empty_and_set() {
  ComplicationRegistry registry;
  registry.setSlotSource(0, ComplicationSource::kDroidName);

  ScreenBuffer screen;
  registry.render(&screen);
  TEST_ASSERT_EQUAL_STRING("(no droid)", screen.line(0));

  ComplicationData data;
  std::strncpy(data.droidName, "R2-D2", sizeof(data.droidName));
  registry.setData(data);
  registry.render(&screen);
  TEST_ASSERT_EQUAL_STRING("R2-D2", screen.line(0));
}

void test_render_handedness_all_variants() {
  ComplicationRegistry registry;
  registry.setSlotSource(0, ComplicationSource::kHandedness);
  ScreenBuffer screen;

  ComplicationData data;
  data.handedness = DownlinkPacket::Handedness::kLeft;
  registry.setData(data);
  registry.render(&screen);
  TEST_ASSERT_EQUAL_STRING("Hand: Left", screen.line(0));

  data.handedness = DownlinkPacket::Handedness::kRight;
  registry.setData(data);
  registry.render(&screen);
  TEST_ASSERT_EQUAL_STRING("Hand: Right", screen.line(0));

  data.handedness = DownlinkPacket::Handedness::kUnknown;
  registry.setData(data);
  registry.render(&screen);
  TEST_ASSERT_EQUAL_STRING("Hand: ?", screen.line(0));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_source_labels);
  RUN_TEST(test_default_slot_assignments);
  RUN_TEST(test_set_slot_source);
  RUN_TEST(test_set_slot_source_out_of_range_is_noop);
  RUN_TEST(test_slot_source_out_of_range_returns_default);
  RUN_TEST(test_cycle_slot_source_advances_and_wraps);
  RUN_TEST(test_cycle_slot_source_out_of_range_is_noop);
  RUN_TEST(test_render_battery);
  RUN_TEST(test_render_battery_blanks_when_indicator_hidden);
  RUN_TEST(test_render_left_and_right_slot_with_labels);
  RUN_TEST(test_render_left_slot_falls_back_when_label_empty);
  RUN_TEST(test_render_signal_known_and_unknown);
  RUN_TEST(test_render_droid_name_empty_and_set);
  RUN_TEST(test_render_handedness_all_variants);
  return UNITY_END();
}
