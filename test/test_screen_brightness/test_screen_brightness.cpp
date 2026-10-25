#include <unity.h>

#include "screen_brightness.h"

void setUp(void) {}
void tearDown(void) {}

void test_contrast_increases_every_step() {
  for (int percent = 20; percent <= 100; percent += 10) {
    TEST_ASSERT_TRUE(screenDriveFor(percent, false).contrast >
                     screenDriveFor(percent - 10, false).contrast);
  }
}

void test_contrast_is_linear_up_to_full() {
  TEST_ASSERT_EQUAL_HEX8(0x1A, screenDriveFor(10, false).contrast);
  TEST_ASSERT_EQUAL_HEX8(0x80, screenDriveFor(50, false).contrast);
  TEST_ASSERT_EQUAL_HEX8(0xFF, screenDriveFor(100, false).contrast);
}

void test_default_80_percent_is_close_to_adafruit_default_contrast() {
  // Adafruit_SSD1306 sets 0xCF at begin(), the look before this setting.
  TEST_ASSERT_EQUAL_HEX8(0xCC, screenDriveFor(80, false).contrast);
}

void test_undimmed_keeps_adafruit_precharge_and_vcomh() {
  for (int percent = 10; percent <= 100; percent += 10) {
    const ScreenDrive drive = screenDriveFor(percent, false);
    TEST_ASSERT_EQUAL_HEX8(0xF1, drive.precharge);
    TEST_ASSERT_EQUAL_HEX8(0x40, drive.vcomh);
  }
}

// Contrast 0 alone (all the dim stage used to do) was barely dimmer than
// the normal look.
void test_dimmed_also_lowers_precharge_and_vcomh() {
  const ScreenDrive dim = screenDriveFor(80, true);
  TEST_ASSERT_EQUAL_HEX8(0x00, dim.contrast);
  TEST_ASSERT_EQUAL_HEX8(0x11, dim.precharge);
  TEST_ASSERT_EQUAL_HEX8(0x00, dim.vcomh);
}

// Regression: waking from the dim stage restored Adafruit_SSD1306's fixed
// default contrast, so a brightness setting would have been lost on wake.
void test_dimmed_ignores_setting_and_undimmed_returns_to_it() {
  const ScreenDrive dim = screenDriveFor(10, true);
  for (int percent = 10; percent <= 100; percent += 10) {
    TEST_ASSERT_TRUE(dim == screenDriveFor(percent, true));
    TEST_ASSERT_FALSE(dim == screenDriveFor(percent, false));
    TEST_ASSERT_TRUE(screenDriveFor(percent, false).contrast > dim.contrast);
  }
}

void test_out_of_range_percent_is_clamped() {
  TEST_ASSERT_EQUAL_HEX8(0xFF, screenDriveFor(999, false).contrast);
  TEST_ASSERT_EQUAL_HEX8(0x1A, screenDriveFor(0, false).contrast);
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_contrast_increases_every_step);
  RUN_TEST(test_contrast_is_linear_up_to_full);
  RUN_TEST(test_default_80_percent_is_close_to_adafruit_default_contrast);
  RUN_TEST(test_undimmed_keeps_adafruit_precharge_and_vcomh);
  RUN_TEST(test_dimmed_also_lowers_precharge_and_vcomh);
  RUN_TEST(test_dimmed_ignores_setting_and_undimmed_returns_to_it);
  RUN_TEST(test_out_of_range_percent_is_clamped);
  return UNITY_END();
}
