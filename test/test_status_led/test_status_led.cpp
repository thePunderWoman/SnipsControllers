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

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_booting_is_blue);
  RUN_TEST(test_connected_is_green);
  RUN_TEST(test_disconnected_is_yellow);
  RUN_TEST(test_charging_is_orange);
  RUN_TEST(test_error_is_red);
  RUN_TEST(test_unknown_state_is_off);
  return UNITY_END();
}
