#include <unity.h>

#include "activity.h"

void setUp(void) {}
void tearDown(void) {}

namespace {

CalibrationData calibratedData() {
  CalibrationData data;
  data.triggerMin = 300;
  data.triggerMax = 3500;
  data.stickXMin = 500;
  data.stickXCenter = 2000;
  data.stickXMax = 3500;
  data.stickYMin = 600;
  data.stickYCenter = 2100;
  data.stickYMax = 3600;
  return data;
}

}  // namespace

// ---- calibration checks ---------------------------------------------------

void test_defaults_are_not_calibrated() {
  const CalibrationData data;
  TEST_ASSERT_FALSE(isTriggerCalibrated(data));
  TEST_ASSERT_FALSE(isStickCalibrated(data));
}

void test_saved_calibration_is_calibrated() {
  const CalibrationData data = calibratedData();
  TEST_ASSERT_TRUE(isTriggerCalibrated(data));
  TEST_ASSERT_TRUE(isStickCalibrated(data));
}

void test_deadzone_change_alone_is_not_stick_calibration() {
  CalibrationData data;
  data.stickDeadzonePercent = 12;
  TEST_ASSERT_FALSE(isStickCalibrated(data));
}

// ---- trigger --------------------------------------------------------------

// Regression: with no trigger mechanism installed, the hall sensor rests
// mid-scale, which the default calibration reads as ~50% pulled. That
// counted as activity every tick, so low-power mode never saw idle.
void test_uncalibrated_trigger_never_counts() {
  ActivityDetector detector;
  detector.setCalibration(CalibrationData());
  ActivityInputs inputs;
  inputs.triggerPercent = 50;
  TEST_ASSERT_EQUAL_UINT8(0, detector.update(inputs));
}

void test_calibrated_trigger_counts_while_pulled() {
  ActivityDetector detector;
  detector.setCalibration(calibratedData());
  ActivityInputs inputs;
  inputs.triggerPercent = 4;
  TEST_ASSERT_EQUAL_UINT8(0, detector.update(inputs));
  inputs.triggerPercent = 5;
  TEST_ASSERT_EQUAL_UINT8(ActivitySources::kTrigger, detector.update(inputs));
  TEST_ASSERT_EQUAL_UINT8(ActivitySources::kTrigger, detector.update(inputs));
}

// ---- stick ----------------------------------------------------------------

void test_calibrated_stick_counts_while_held_off_center() {
  ActivityDetector detector;
  detector.setCalibration(calibratedData());
  ActivityInputs inputs;
  inputs.stickXPercent = 3;
  TEST_ASSERT_EQUAL_UINT8(0, detector.update(inputs));
  inputs.stickYPercent = -5;
  TEST_ASSERT_EQUAL_UINT8(ActivitySources::kStick, detector.update(inputs));
  TEST_ASSERT_EQUAL_UINT8(ActivitySources::kStick, detector.update(inputs));
}

// Regression: an uncalibrated stick resting away from the default 2048
// center read as permanently deflected.
void test_uncalibrated_stick_at_rest_off_default_center_does_not_count() {
  ActivityDetector detector;
  detector.setCalibration(CalibrationData());
  ActivityInputs inputs;
  inputs.rawStickX = 1700;
  inputs.rawStickY = 2500;
  inputs.stickXPercent = -17;  // what the default calibration reports
  inputs.stickYPercent = 20;
  TEST_ASSERT_EQUAL_UINT8(0, detector.update(inputs));
  inputs.rawStickX = 1750;  // ADC noise
  TEST_ASSERT_EQUAL_UINT8(0, detector.update(inputs));
}

void test_uncalibrated_stick_counts_when_moved() {
  ActivityDetector detector;
  detector.setCalibration(CalibrationData());
  ActivityInputs inputs;
  inputs.rawStickX = 1700;
  inputs.rawStickY = 2500;
  detector.update(inputs);

  inputs.rawStickY = 2700;
  TEST_ASSERT_EQUAL_UINT8(ActivitySources::kStick, detector.update(inputs));
  // Measured from where it last counted, so holding still stops counting.
  TEST_ASSERT_EQUAL_UINT8(0, detector.update(inputs));
  inputs.rawStickX = 1500;
  TEST_ASSERT_EQUAL_UINT8(ActivitySources::kStick, detector.update(inputs));
}

void test_set_calibration_resets_uncalibrated_stick_reference() {
  ActivityDetector detector;
  detector.setCalibration(CalibrationData());
  ActivityInputs inputs;
  inputs.rawStickX = 1000;
  detector.update(inputs);

  detector.setCalibration(CalibrationData());
  inputs.rawStickX = 3000;  // first reading after reset just sets rest
  TEST_ASSERT_EQUAL_UINT8(0, detector.update(inputs));
}

// ---- buttons, motion, edges ----------------------------------------------

void test_button_and_motion_count() {
  ActivityDetector detector;
  ActivityInputs inputs;
  inputs.anyButtonHeld = true;
  inputs.motion = true;
  TEST_ASSERT_EQUAL_UINT8(ActivitySources::kButton | ActivitySources::kMotion,
                          detector.update(inputs));
}

void test_newly_active_sources_reports_only_rising_edges() {
  ActivityDetector detector;
  ActivityInputs inputs;
  inputs.motion = true;
  detector.update(inputs);
  TEST_ASSERT_EQUAL_UINT8(ActivitySources::kMotion,
                          detector.newlyActiveSources());

  inputs.anyButtonHeld = true;
  detector.update(inputs);
  TEST_ASSERT_EQUAL_UINT8(ActivitySources::kButton,
                          detector.newlyActiveSources());

  detector.update(inputs);
  TEST_ASSERT_EQUAL_UINT8(0, detector.newlyActiveSources());

  inputs.motion = false;
  detector.update(inputs);
  inputs.motion = true;
  detector.update(inputs);
  TEST_ASSERT_EQUAL_UINT8(ActivitySources::kMotion,
                          detector.newlyActiveSources());
}

// ---- describeActivitySources ---------------------------------------------

void test_describe_no_sources() {
  char buf[32];
  describeActivitySources(0, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("none", buf);
}

void test_describe_one_source() {
  char buf[32];
  describeActivitySources(ActivitySources::kTrigger, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("trigger", buf);
}

void test_describe_all_sources() {
  char buf[40];
  describeActivitySources(ActivitySources::kButton | ActivitySources::kStick |
                              ActivitySources::kTrigger |
                              ActivitySources::kMotion,
                          buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("button, stick, trigger, motion", buf);
}

void test_describe_truncates_safely() {
  char buf[10];
  describeActivitySources(ActivitySources::kButton | ActivitySources::kStick |
                              ActivitySources::kTrigger,
                          buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("button, s", buf);
}

void test_describe_zero_capacity_writes_nothing() {
  char buf[1] = {'x'};
  describeActivitySources(ActivitySources::kButton, buf, 0);
  TEST_ASSERT_EQUAL_CHAR('x', buf[0]);
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_defaults_are_not_calibrated);
  RUN_TEST(test_saved_calibration_is_calibrated);
  RUN_TEST(test_deadzone_change_alone_is_not_stick_calibration);
  RUN_TEST(test_uncalibrated_trigger_never_counts);
  RUN_TEST(test_calibrated_trigger_counts_while_pulled);
  RUN_TEST(test_calibrated_stick_counts_while_held_off_center);
  RUN_TEST(test_uncalibrated_stick_at_rest_off_default_center_does_not_count);
  RUN_TEST(test_uncalibrated_stick_counts_when_moved);
  RUN_TEST(test_set_calibration_resets_uncalibrated_stick_reference);
  RUN_TEST(test_button_and_motion_count);
  RUN_TEST(test_newly_active_sources_reports_only_rising_edges);
  RUN_TEST(test_describe_no_sources);
  RUN_TEST(test_describe_one_source);
  RUN_TEST(test_describe_all_sources);
  RUN_TEST(test_describe_truncates_safely);
  RUN_TEST(test_describe_zero_capacity_writes_nothing);
  return UNITY_END();
}
