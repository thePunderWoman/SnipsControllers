#include <unity.h>

#include "screen_brightness.h"

void setUp(void) {}
void tearDown(void) {}

void test_next_screen_brightness_cycles_and_wraps() {
  TEST_ASSERT_TRUE(ScreenBrightness::kMedium ==
                   nextScreenBrightness(ScreenBrightness::kLow));
  TEST_ASSERT_TRUE(ScreenBrightness::kHigh ==
                   nextScreenBrightness(ScreenBrightness::kMedium));
  TEST_ASSERT_TRUE(ScreenBrightness::kLow ==
                   nextScreenBrightness(ScreenBrightness::kHigh));
}

void test_next_screen_brightness_out_of_range_restarts_at_low() {
  TEST_ASSERT_TRUE(ScreenBrightness::kLow ==
                   nextScreenBrightness(static_cast<ScreenBrightness>(99)));
  TEST_ASSERT_TRUE(ScreenBrightness::kLow ==
                   nextScreenBrightness(static_cast<ScreenBrightness>(-5)));
}

void test_contrast_increases_with_brightness() {
  const uint8_t low = screenContrastFor(ScreenBrightness::kLow, false);
  const uint8_t medium = screenContrastFor(ScreenBrightness::kMedium, false);
  const uint8_t high = screenContrastFor(ScreenBrightness::kHigh, false);
  TEST_ASSERT_TRUE(low < medium);
  TEST_ASSERT_TRUE(medium < high);
}

void test_high_matches_adafruit_default_contrast() {
  TEST_ASSERT_EQUAL_HEX8(0xCF, screenContrastFor(ScreenBrightness::kHigh, false));
}

// Regression: waking from the dim tier restored Adafruit_SSD1306's fixed
// default contrast, so a brightness setting would have been lost on wake.
void test_dimmed_is_lowest_and_undimmed_returns_to_setting() {
  for (int i = 0; i < static_cast<int>(ScreenBrightness::kCount); ++i) {
    const auto brightness = static_cast<ScreenBrightness>(i);
    TEST_ASSERT_EQUAL_HEX8(0x00, screenContrastFor(brightness, true));
    TEST_ASSERT_TRUE(screenContrastFor(brightness, false) >
                     screenContrastFor(brightness, true));
  }
}

void test_out_of_range_gets_high_contrast() {
  TEST_ASSERT_EQUAL_HEX8(
      0xCF, screenContrastFor(static_cast<ScreenBrightness>(99), false));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_next_screen_brightness_cycles_and_wraps);
  RUN_TEST(test_next_screen_brightness_out_of_range_restarts_at_low);
  RUN_TEST(test_contrast_increases_with_brightness);
  RUN_TEST(test_high_matches_adafruit_default_contrast);
  RUN_TEST(test_dimmed_is_lowest_and_undimmed_returns_to_setting);
  RUN_TEST(test_out_of_range_gets_high_contrast);
  return UNITY_END();
}
