#include <unity.h>
#include <cstring>

#include "text_entry.h"

void setUp(void) {}
void tearDown(void) {}

void test_starts_empty_and_not_done() {
  TextEntryWidget widget(TextEntryWidget::CharSet::kAlphanumeric);
  TEST_ASSERT_EQUAL_STRING("", widget.text());
  TEST_ASSERT_EQUAL_INT(0, widget.length());
  TEST_ASSERT_FALSE(widget.done());
  TEST_ASSERT_FALSE(widget.isDoneSelected());
}

void test_starts_highlighting_first_charset_char() {
  TextEntryWidget widget(TextEntryWidget::CharSet::kAlphanumeric);
  // First alphanumeric charset character is a space.
  TEST_ASSERT_EQUAL_INT(' ', widget.currentChar());
}

void test_scroll_next_advances_through_charset() {
  TextEntryWidget widget(TextEntryWidget::CharSet::kAlphanumeric);
  widget.scrollNext();  // space -> 'A'
  TEST_ASSERT_EQUAL_INT('A', widget.currentChar());
  widget.scrollNext();  // 'A' -> 'B'
  TEST_ASSERT_EQUAL_INT('B', widget.currentChar());
}

void test_scroll_prev_from_start_wraps_to_done() {
  TextEntryWidget widget(TextEntryWidget::CharSet::kAlphanumeric);
  widget.scrollPrev();
  TEST_ASSERT_TRUE(widget.isDoneSelected());
}

void test_scroll_next_from_last_char_wraps_to_done_then_back_to_start() {
  TextEntryWidget widget(TextEntryWidget::CharSet::kHex);
  // Hex charset has 16 entries (0-9, A-F); scroll to the last one ('F').
  for (int i = 0; i < 15; ++i) widget.scrollNext();
  TEST_ASSERT_EQUAL_INT('F', widget.currentChar());

  widget.scrollNext();  // 'F' -> done
  TEST_ASSERT_TRUE(widget.isDoneSelected());

  widget.scrollNext();  // done -> wraps back to '0'
  TEST_ASSERT_EQUAL_INT('0', widget.currentChar());
}

void test_commit_char_appends_and_resets_highlight() {
  TextEntryWidget widget(TextEntryWidget::CharSet::kHex);
  widget.scrollNext();  // '0' -> '1'
  widget.commitChar();
  TEST_ASSERT_EQUAL_STRING("1", widget.text());
  TEST_ASSERT_EQUAL_INT(1, widget.length());
  // Highlight resets to the first charset character for the next slot.
  TEST_ASSERT_EQUAL_INT('0', widget.currentChar());
}

void test_commit_multiple_chars_builds_string() {
  TextEntryWidget widget(TextEntryWidget::CharSet::kHex);
  widget.scrollNext();
  widget.scrollNext();
  widget.commitChar();  // '2'
  widget.scrollNext();
  widget.scrollNext();
  widget.scrollNext();
  widget.scrollNext();
  widget.commitChar();  // '4'
  TEST_ASSERT_EQUAL_STRING("24", widget.text());
}

void test_commit_done_finishes_without_appending() {
  TextEntryWidget widget(TextEntryWidget::CharSet::kHex);
  widget.commitChar();  // '0'
  widget.scrollPrev();  // '0' -> done
  TEST_ASSERT_TRUE(widget.isDoneSelected());
  widget.commitChar();
  TEST_ASSERT_TRUE(widget.done());
  TEST_ASSERT_EQUAL_STRING("0", widget.text());
}

void test_backspace_removes_last_char() {
  TextEntryWidget widget(TextEntryWidget::CharSet::kHex);
  widget.commitChar();  // '0'
  widget.scrollNext();
  widget.commitChar();  // '1'
  widget.backspace();
  TEST_ASSERT_EQUAL_STRING("0", widget.text());
  TEST_ASSERT_EQUAL_INT(1, widget.length());
}

void test_backspace_on_empty_is_noop() {
  TextEntryWidget widget(TextEntryWidget::CharSet::kHex);
  widget.backspace();
  TEST_ASSERT_EQUAL_STRING("", widget.text());
}

void test_commit_char_respects_max_length() {
  TextEntryWidget widget(TextEntryWidget::CharSet::kHex, 2);
  widget.commitChar();  // '0'
  widget.commitChar();  // '0'
  TEST_ASSERT_EQUAL_INT(2, widget.length());
  // Buffer is full — committing a non-done char is a no-op.
  widget.commitChar();
  TEST_ASSERT_EQUAL_INT(2, widget.length());
  TEST_ASSERT_EQUAL_STRING("00", widget.text());
}

void test_can_still_finish_via_done_when_buffer_full() {
  TextEntryWidget widget(TextEntryWidget::CharSet::kHex, 1);
  widget.commitChar();  // '0', now full
  widget.scrollPrev();  // '0' -> done
  widget.commitChar();
  TEST_ASSERT_TRUE(widget.done());
  TEST_ASSERT_EQUAL_STRING("0", widget.text());
}

void test_all_actions_are_noop_once_done() {
  TextEntryWidget widget(TextEntryWidget::CharSet::kHex);
  widget.commitChar();  // '0'
  widget.scrollPrev();  // -> done
  widget.commitChar();  // finishes
  TEST_ASSERT_TRUE(widget.done());

  widget.scrollNext();
  widget.scrollPrev();
  widget.commitChar();
  widget.backspace();
  TEST_ASSERT_EQUAL_STRING("0", widget.text());
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_starts_empty_and_not_done);
  RUN_TEST(test_starts_highlighting_first_charset_char);
  RUN_TEST(test_scroll_next_advances_through_charset);
  RUN_TEST(test_scroll_prev_from_start_wraps_to_done);
  RUN_TEST(test_scroll_next_from_last_char_wraps_to_done_then_back_to_start);
  RUN_TEST(test_commit_char_appends_and_resets_highlight);
  RUN_TEST(test_commit_multiple_chars_builds_string);
  RUN_TEST(test_commit_done_finishes_without_appending);
  RUN_TEST(test_backspace_removes_last_char);
  RUN_TEST(test_backspace_on_empty_is_noop);
  RUN_TEST(test_commit_char_respects_max_length);
  RUN_TEST(test_can_still_finish_via_done_when_buffer_full);
  RUN_TEST(test_all_actions_are_noop_once_done);
  return UNITY_END();
}
