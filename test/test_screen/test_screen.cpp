#include <unity.h>
#include <cstring>

#include "screen.h"

void setUp(void) {}
void tearDown(void) {}

void test_new_buffer_lines_are_empty() {
  ScreenBuffer buffer;
  for (size_t i = 0; i < ScreenBuffer::lineCount(); ++i) {
    TEST_ASSERT_EQUAL_STRING("", buffer.line(i));
  }
}

void test_set_line_is_retrievable() {
  ScreenBuffer buffer;
  buffer.setLine(0, "Snips Controller");
  TEST_ASSERT_EQUAL_STRING("Snips Controller", buffer.line(0));
}

void test_set_line_truncates_text_too_long_for_display() {
  ScreenBuffer buffer;
  // 25 'x's — longer than kMaxLineLength (21).
  buffer.setLine(0, "xxxxxxxxxxxxxxxxxxxxxxxxx");
  TEST_ASSERT_EQUAL_INT(ScreenBuffer::kMaxLineLength,
                        std::strlen(buffer.line(0)));
}

void test_set_line_ignores_out_of_range_index() {
  ScreenBuffer buffer;
  buffer.setLine(ScreenBuffer::kMaxLines + 5, "should be ignored");
  // Nothing should have been written anywhere retrievable.
  for (size_t i = 0; i < ScreenBuffer::lineCount(); ++i) {
    TEST_ASSERT_EQUAL_STRING("", buffer.line(i));
  }
}

void test_set_line_ignores_null_text() {
  ScreenBuffer buffer;
  buffer.setLine(0, "kept");
  buffer.setLine(0, nullptr);
  TEST_ASSERT_EQUAL_STRING("kept", buffer.line(0));
}

void test_clear_resets_all_lines() {
  ScreenBuffer buffer;
  buffer.setLine(0, "line 0");
  buffer.setLine(3, "line 3");
  buffer.clear();
  for (size_t i = 0; i < ScreenBuffer::lineCount(); ++i) {
    TEST_ASSERT_EQUAL_STRING("", buffer.line(i));
  }
}

void test_line_out_of_range_returns_empty_string() {
  ScreenBuffer buffer;
  TEST_ASSERT_EQUAL_STRING("", buffer.line(ScreenBuffer::kMaxLines + 5));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_new_buffer_lines_are_empty);
  RUN_TEST(test_set_line_is_retrievable);
  RUN_TEST(test_set_line_truncates_text_too_long_for_display);
  RUN_TEST(test_set_line_ignores_out_of_range_index);
  RUN_TEST(test_set_line_ignores_null_text);
  RUN_TEST(test_clear_resets_all_lines);
  RUN_TEST(test_line_out_of_range_returns_empty_string);
  return UNITY_END();
}
