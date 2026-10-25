#include <unity.h>

#include "status_led.h"

void setUp(void) {}
void tearDown(void) {}

void test_booting_is_blue() {
  StatusLedController controller;
  RgbColor color = controller.colorFor(SystemState::kBooting);
  TEST_ASSERT_EQUAL_UINT8(0, color.r);
  TEST_ASSERT_EQUAL_UINT8(0, color.g);
  TEST_ASSERT_EQUAL_UINT8(255, color.b);
}

void test_connected_is_green() {
  StatusLedController controller;
  RgbColor color = controller.colorFor(SystemState::kConnected);
  TEST_ASSERT_EQUAL_UINT8(0, color.r);
  TEST_ASSERT_EQUAL_UINT8(255, color.g);
  TEST_ASSERT_EQUAL_UINT8(0, color.b);
}

void test_disconnected_is_yellow() {
  StatusLedController controller;
  RgbColor color = controller.colorFor(SystemState::kDisconnected);
  TEST_ASSERT_EQUAL_UINT8(255, color.r);
  TEST_ASSERT_EQUAL_UINT8(255, color.g);
  TEST_ASSERT_EQUAL_UINT8(0, color.b);
}

void test_charging_is_orange() {
  StatusLedController controller;
  RgbColor color = controller.colorFor(SystemState::kCharging);
  TEST_ASSERT_EQUAL_UINT8(255, color.r);
  TEST_ASSERT_EQUAL_UINT8(165, color.g);
  TEST_ASSERT_EQUAL_UINT8(0, color.b);
}

void test_error_is_red() {
  StatusLedController controller;
  RgbColor color = controller.colorFor(SystemState::kError);
  TEST_ASSERT_EQUAL_UINT8(255, color.r);
  TEST_ASSERT_EQUAL_UINT8(0, color.g);
  TEST_ASSERT_EQUAL_UINT8(0, color.b);
}

void test_unknown_state_is_off() {
  StatusLedController controller;
  // A value outside the enum's defined range — the defensive fallback for
  // whatever calls colorFor() with something invalid.
  RgbColor color = controller.colorFor(static_cast<SystemState>(99));
  TEST_ASSERT_EQUAL_UINT8(0, color.r);
  TEST_ASSERT_EQUAL_UINT8(0, color.g);
  TEST_ASSERT_EQUAL_UINT8(0, color.b);
}

void test_next_led_brightness_cycles_and_wraps() {
  TEST_ASSERT_TRUE(LedBrightness::kMedium == nextLedBrightness(LedBrightness::kLow));
  TEST_ASSERT_TRUE(LedBrightness::kHigh == nextLedBrightness(LedBrightness::kMedium));
  TEST_ASSERT_TRUE(LedBrightness::kLow == nextLedBrightness(LedBrightness::kHigh));
}

void test_next_led_brightness_recovers_from_out_of_range_value() {
  TEST_ASSERT_TRUE(LedBrightness::kLow ==
                   nextLedBrightness(static_cast<LedBrightness>(99)));
  TEST_ASSERT_TRUE(LedBrightness::kLow ==
                   nextLedBrightness(static_cast<LedBrightness>(-1)));
}

void test_led_brightness_scales_increase_but_stay_below_full() {
  TEST_ASSERT_EQUAL_UINT8(10, ledBrightnessScale(LedBrightness::kLow));
  TEST_ASSERT_EQUAL_UINT8(31, ledBrightnessScale(LedBrightness::kMedium));
  TEST_ASSERT_EQUAL_UINT8(77, ledBrightnessScale(LedBrightness::kHigh));
}

void test_out_of_range_led_brightness_uses_low_scale() {
  TEST_ASSERT_EQUAL_UINT8(
      10, ledBrightnessScale(static_cast<LedBrightness>(99)));
}

void test_scale_color_full_scale_is_unchanged() {
  RgbColor color = scaleColor({255, 165, 0}, 255);
  TEST_ASSERT_EQUAL_UINT8(255, color.r);
  TEST_ASSERT_EQUAL_UINT8(165, color.g);
  TEST_ASSERT_EQUAL_UINT8(0, color.b);
}

void test_scale_color_rounds_each_channel() {
  // Orange at kLow: 255*51/255 = 51, 165*51/255 = 33.
  RgbColor color = scaleColor({255, 165, 0}, 51);
  TEST_ASSERT_EQUAL_UINT8(51, color.r);
  TEST_ASSERT_EQUAL_UINT8(33, color.g);
  TEST_ASSERT_EQUAL_UINT8(0, color.b);
  // 3*128/255 = 1.506, rounds up to 2.
  TEST_ASSERT_EQUAL_UINT8(2, scaleColor({3, 0, 0}, 128).r);
}

void test_scale_color_zero_scale_is_off() {
  RgbColor color = scaleColor({255, 255, 255}, 0);
  TEST_ASSERT_EQUAL_UINT8(0, color.r);
  TEST_ASSERT_EQUAL_UINT8(0, color.g);
  TEST_ASSERT_EQUAL_UINT8(0, color.b);
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_booting_is_blue);
  RUN_TEST(test_connected_is_green);
  RUN_TEST(test_disconnected_is_yellow);
  RUN_TEST(test_charging_is_orange);
  RUN_TEST(test_error_is_red);
  RUN_TEST(test_unknown_state_is_off);
  RUN_TEST(test_next_led_brightness_cycles_and_wraps);
  RUN_TEST(test_next_led_brightness_recovers_from_out_of_range_value);
  RUN_TEST(test_led_brightness_scales_increase_but_stay_below_full);
  RUN_TEST(test_out_of_range_led_brightness_uses_low_scale);
  RUN_TEST(test_scale_color_full_scale_is_unchanged);
  RUN_TEST(test_scale_color_rounds_each_channel);
  RUN_TEST(test_scale_color_zero_scale_is_off);
  return UNITY_END();
}
