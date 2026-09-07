#include <unity.h>

#include "buttons.h"

void setUp(void) {}
void tearDown(void) {}

// ---- ButtonDebouncer --------------------------------------------------

void test_debouncer_starts_not_pressed() {
  ButtonDebouncer debouncer(20);
  TEST_ASSERT_FALSE(debouncer.isPressed());
}

void test_debouncer_ignores_press_shorter_than_threshold() {
  ButtonDebouncer debouncer(20);
  TEST_ASSERT_FALSE(debouncer.update(true, 0));
  TEST_ASSERT_FALSE(debouncer.update(true, 10));
  TEST_ASSERT_FALSE(debouncer.update(true, 19));
}

void test_debouncer_registers_press_held_past_threshold() {
  ButtonDebouncer debouncer(20);
  debouncer.update(true, 0);
  TEST_ASSERT_TRUE(debouncer.update(true, 20));
  TEST_ASSERT_TRUE(debouncer.isPressed());
}

void test_debouncer_release_is_also_debounced() {
  ButtonDebouncer debouncer(20);
  debouncer.update(true, 0);
  TEST_ASSERT_TRUE(debouncer.update(true, 20));

  // Release starts, but hasn't been stable long enough yet.
  TEST_ASSERT_TRUE(debouncer.update(false, 25));
  TEST_ASSERT_TRUE(debouncer.update(false, 30));
  // Now stable for >= 20ms since the release began at t=25.
  TEST_ASSERT_FALSE(debouncer.update(false, 45));
}

void test_debouncer_ignores_bouncing_signal() {
  ButtonDebouncer debouncer(20);
  // Rapid flicker (contact bounce) — raw keeps changing, so the debounce
  // timer keeps restarting and the state never commits to pressed.
  TEST_ASSERT_FALSE(debouncer.update(true, 0));
  TEST_ASSERT_FALSE(debouncer.update(false, 5));
  TEST_ASSERT_FALSE(debouncer.update(true, 10));
  TEST_ASSERT_FALSE(debouncer.update(false, 15));
  TEST_ASSERT_FALSE(debouncer.update(true, 19));
  // Finally holds steady from t=19 onward.
  TEST_ASSERT_TRUE(debouncer.update(true, 39));
}

// ---- Buttons pin table --------------------------------------------------

void test_button_pins_are_unique() {
  for (size_t i = 0; i < Buttons::kCount; ++i) {
    for (size_t j = i + 1; j < Buttons::kCount; ++j) {
      TEST_ASSERT_NOT_EQUAL(Buttons::kPins[i], Buttons::kPins[j]);
    }
  }
}

void test_button_pins_within_gpio_range() {
  for (size_t i = 0; i < Buttons::kCount; ++i) {
    TEST_ASSERT_TRUE(Buttons::kPins[i] >= 0 && Buttons::kPins[i] <= 48);
  }
}

// ---- ButtonPanel ---------------------------------------------------------

void test_panel_defaults_to_not_pressed() {
  ButtonPanel panel;
  TEST_ASSERT_FALSE(panel.isPressed(Buttons::kMacro1));
}

void test_panel_tracks_press_and_release_per_button() {
  ButtonPanel panel;

  panel.update(Buttons::kLeftUp, true, 0);
  panel.update(Buttons::kLeftUp, true, 20);
  TEST_ASSERT_TRUE(panel.isPressed(Buttons::kLeftUp));
  // A different button is unaffected.
  TEST_ASSERT_FALSE(panel.isPressed(Buttons::kRightDown));

  panel.update(Buttons::kLeftUp, false, 25);
  panel.update(Buttons::kLeftUp, false, 45);
  TEST_ASSERT_FALSE(panel.isPressed(Buttons::kLeftUp));
}

void test_panel_ignores_out_of_range_index() {
  ButtonPanel panel;
  panel.update(Buttons::kCount + 5, true, 100);
  TEST_ASSERT_FALSE(panel.isPressed(Buttons::kCount + 5));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_debouncer_starts_not_pressed);
  RUN_TEST(test_debouncer_ignores_press_shorter_than_threshold);
  RUN_TEST(test_debouncer_registers_press_held_past_threshold);
  RUN_TEST(test_debouncer_release_is_also_debounced);
  RUN_TEST(test_debouncer_ignores_bouncing_signal);
  RUN_TEST(test_button_pins_are_unique);
  RUN_TEST(test_button_pins_within_gpio_range);
  RUN_TEST(test_panel_defaults_to_not_pressed);
  RUN_TEST(test_panel_tracks_press_and_release_per_button);
  RUN_TEST(test_panel_ignores_out_of_range_index);
  return UNITY_END();
}
