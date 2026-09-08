#include <unity.h>

#include "calibration.h"

void setUp(void) {}
void tearDown(void) {}

// ---- AnalogCalibration::calibrateTrigger ---------------------------------

void test_trigger_at_min_is_zero_percent() {
  CalibrationData data;
  TEST_ASSERT_EQUAL_INT(0, AnalogCalibration::calibrateTrigger(0, data));
}

void test_trigger_at_max_is_hundred_percent() {
  CalibrationData data;
  TEST_ASSERT_EQUAL_INT(100, AnalogCalibration::calibrateTrigger(4095, data));
}

void test_trigger_at_midpoint_is_about_fifty_percent() {
  CalibrationData data;
  TEST_ASSERT_EQUAL_INT(50, AnalogCalibration::calibrateTrigger(2048, data));
}

void test_trigger_clamps_beyond_max() {
  CalibrationData data;
  TEST_ASSERT_EQUAL_INT(100, AnalogCalibration::calibrateTrigger(5000, data));
}

void test_trigger_clamps_below_min() {
  CalibrationData data;
  TEST_ASSERT_EQUAL_INT(0, AnalogCalibration::calibrateTrigger(-100, data));
}

void test_trigger_degenerate_range_returns_zero() {
  CalibrationData data;
  data.triggerMin = 100;
  data.triggerMax = 100;
  TEST_ASSERT_EQUAL_INT(0, AnalogCalibration::calibrateTrigger(2048, data));
}

// ---- AnalogCalibration::calibrateStickAxis --------------------------------

void test_stick_axis_at_center_is_zero() {
  TEST_ASSERT_EQUAL_INT(
      0, AnalogCalibration::calibrateStickAxis(2048, 0, 2048, 4095));
}

void test_stick_axis_within_deadzone_is_zero() {
  // Deadzone half-width for this range is (4095-0)*3/100 = 122.85.
  TEST_ASSERT_EQUAL_INT(
      0, AnalogCalibration::calibrateStickAxis(2170, 0, 2048, 4095));
}

void test_stick_axis_just_outside_deadzone_is_nonzero() {
  TEST_ASSERT_EQUAL_INT(
      6, AnalogCalibration::calibrateStickAxis(2171, 0, 2048, 4095));
}

void test_stick_axis_at_max_is_hundred() {
  TEST_ASSERT_EQUAL_INT(
      100, AnalogCalibration::calibrateStickAxis(4095, 0, 2048, 4095));
}

void test_stick_axis_at_min_is_negative_hundred() {
  TEST_ASSERT_EQUAL_INT(
      -100, AnalogCalibration::calibrateStickAxis(0, 0, 2048, 4095));
}

void test_stick_axis_clamps_beyond_max() {
  TEST_ASSERT_EQUAL_INT(
      100, AnalogCalibration::calibrateStickAxis(6000, 0, 2048, 4095));
}

void test_stick_axis_clamps_below_min() {
  TEST_ASSERT_EQUAL_INT(
      -100, AnalogCalibration::calibrateStickAxis(-500, 0, 2048, 4095));
}

void test_stick_axis_degenerate_max_at_center_returns_zero() {
  TEST_ASSERT_EQUAL_INT(
      0, AnalogCalibration::calibrateStickAxis(1000, 0, 4095, 4095));
}

void test_stick_axis_degenerate_center_at_min_returns_zero() {
  TEST_ASSERT_EQUAL_INT(
      0, AnalogCalibration::calibrateStickAxis(3000, 2048, 2048, 4095));
}

// ---- TriggerCalibrationFlow ------------------------------------------------

void test_trigger_flow_starts_awaiting_release() {
  TriggerCalibrationFlow flow;
  TEST_ASSERT_TRUE(TriggerCalibrationFlow::Step::kAwaitingRelease ==
                    flow.currentStep());
}

void test_trigger_flow_captures_min_then_max_then_done() {
  TriggerCalibrationFlow flow;
  flow.confirmStep(50);
  TEST_ASSERT_TRUE(TriggerCalibrationFlow::Step::kAwaitingFullPull ==
                    flow.currentStep());
  TEST_ASSERT_EQUAL_INT(50, flow.min());

  flow.confirmStep(4000);
  TEST_ASSERT_TRUE(TriggerCalibrationFlow::Step::kDone == flow.currentStep());
  TEST_ASSERT_EQUAL_INT(4000, flow.max());
}

void test_trigger_flow_ignores_confirm_once_done() {
  TriggerCalibrationFlow flow;
  flow.confirmStep(50);
  flow.confirmStep(4000);
  flow.confirmStep(9999);
  TEST_ASSERT_TRUE(TriggerCalibrationFlow::Step::kDone == flow.currentStep());
  TEST_ASSERT_EQUAL_INT(50, flow.min());
  TEST_ASSERT_EQUAL_INT(4000, flow.max());
}

// ---- StickCalibrationFlow ---------------------------------------------------

void test_stick_flow_starts_awaiting_center() {
  StickCalibrationFlow flow;
  TEST_ASSERT_TRUE(StickCalibrationFlow::Step::kAwaitingCenter ==
                    flow.currentStep());
}

void test_stick_flow_sample_before_center_is_noop() {
  StickCalibrationFlow flow;
  flow.sample(1500, 1500);
  TEST_ASSERT_TRUE(StickCalibrationFlow::Step::kAwaitingCenter ==
                    flow.currentStep());
}

void test_stick_flow_confirm_done_before_rolling_is_noop() {
  StickCalibrationFlow flow;
  flow.confirmDone();
  TEST_ASSERT_TRUE(StickCalibrationFlow::Step::kAwaitingCenter ==
                    flow.currentStep());
}

void test_stick_flow_confirm_center_starts_rolling() {
  StickCalibrationFlow flow;
  flow.confirmCenter(2000, 2100);
  TEST_ASSERT_TRUE(StickCalibrationFlow::Step::kRolling == flow.currentStep());
  TEST_ASSERT_EQUAL_INT(2000, flow.centerX());
  TEST_ASSERT_EQUAL_INT(2100, flow.centerY());
  // Range starts collapsed to the center point itself.
  TEST_ASSERT_EQUAL_INT(2000, flow.minX());
  TEST_ASSERT_EQUAL_INT(2000, flow.maxX());
}

void test_stick_flow_sample_tracks_min_and_max_per_axis() {
  StickCalibrationFlow flow;
  flow.confirmCenter(2000, 2100);
  flow.sample(1500, 2600);
  flow.sample(2500, 1800);

  TEST_ASSERT_EQUAL_INT(1500, flow.minX());
  TEST_ASSERT_EQUAL_INT(2500, flow.maxX());
  TEST_ASSERT_EQUAL_INT(1800, flow.minY());
  TEST_ASSERT_EQUAL_INT(2600, flow.maxY());
}

void test_stick_flow_confirm_center_again_while_rolling_is_noop() {
  StickCalibrationFlow flow;
  flow.confirmCenter(2000, 2100);
  flow.confirmCenter(9999, 9999);
  TEST_ASSERT_EQUAL_INT(2000, flow.centerX());
}

void test_stick_flow_confirm_done_finishes() {
  StickCalibrationFlow flow;
  flow.confirmCenter(2000, 2100);
  flow.sample(1500, 2600);
  flow.confirmDone();
  TEST_ASSERT_TRUE(StickCalibrationFlow::Step::kDone == flow.currentStep());

  // Further samples/confirms are ignored once done.
  flow.sample(0, 0);
  TEST_ASSERT_EQUAL_INT(1500, flow.minX());
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_trigger_at_min_is_zero_percent);
  RUN_TEST(test_trigger_at_max_is_hundred_percent);
  RUN_TEST(test_trigger_at_midpoint_is_about_fifty_percent);
  RUN_TEST(test_trigger_clamps_beyond_max);
  RUN_TEST(test_trigger_clamps_below_min);
  RUN_TEST(test_trigger_degenerate_range_returns_zero);
  RUN_TEST(test_stick_axis_at_center_is_zero);
  RUN_TEST(test_stick_axis_within_deadzone_is_zero);
  RUN_TEST(test_stick_axis_just_outside_deadzone_is_nonzero);
  RUN_TEST(test_stick_axis_at_max_is_hundred);
  RUN_TEST(test_stick_axis_at_min_is_negative_hundred);
  RUN_TEST(test_stick_axis_clamps_beyond_max);
  RUN_TEST(test_stick_axis_clamps_below_min);
  RUN_TEST(test_stick_axis_degenerate_max_at_center_returns_zero);
  RUN_TEST(test_stick_axis_degenerate_center_at_min_returns_zero);
  RUN_TEST(test_trigger_flow_starts_awaiting_release);
  RUN_TEST(test_trigger_flow_captures_min_then_max_then_done);
  RUN_TEST(test_trigger_flow_ignores_confirm_once_done);
  RUN_TEST(test_stick_flow_starts_awaiting_center);
  RUN_TEST(test_stick_flow_sample_before_center_is_noop);
  RUN_TEST(test_stick_flow_confirm_done_before_rolling_is_noop);
  RUN_TEST(test_stick_flow_confirm_center_starts_rolling);
  RUN_TEST(test_stick_flow_sample_tracks_min_and_max_per_axis);
  RUN_TEST(test_stick_flow_confirm_center_again_while_rolling_is_noop);
  RUN_TEST(test_stick_flow_confirm_done_finishes);
  return UNITY_END();
}
