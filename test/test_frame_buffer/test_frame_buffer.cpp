#include <unity.h>

#include "frame_buffer.h"
#include "status_font.h"

void setUp(void) {}
void tearDown(void) {}

int litCount(const FrameBuffer &frame) {
  int count = 0;
  for (int y = 0; y < FrameBuffer::kHeight; ++y) {
    for (int x = 0; x < FrameBuffer::kWidth; ++x) count += frame.pixel(x, y);
  }
  return count;
}

// ---- frame buffer ------------------------------------------------------

void test_new_frame_is_blank() {
  FrameBuffer frame;
  TEST_ASSERT_EQUAL_INT(0, litCount(frame));
}

void test_set_pixel_round_trips() {
  FrameBuffer frame;
  frame.setPixel(5, 9);
  TEST_ASSERT_TRUE(frame.pixel(5, 9));
  TEST_ASSERT_EQUAL_INT(1, litCount(frame));
  frame.setPixel(5, 9, false);
  TEST_ASSERT_FALSE(frame.pixel(5, 9));
}

// oled.cpp copies the bytes straight into Adafruit_SSD1306's buffer, so
// they must be in its layout: byte x + (y / 8) * 128, bit y % 8.
void test_bytes_use_ssd1306_page_layout() {
  FrameBuffer frame;
  frame.setPixel(3, 9);
  TEST_ASSERT_EQUAL_HEX8(0x02, frame.bytes()[3 + 128]);
  frame.setPixel(127, 63);
  TEST_ASSERT_EQUAL_HEX8(0x80, frame.bytes()[FrameBuffer::kByteCount - 1]);
}

void test_off_screen_pixels_are_ignored() {
  FrameBuffer frame;
  frame.setPixel(-1, 0);
  frame.setPixel(0, -1);
  frame.setPixel(FrameBuffer::kWidth, 0);
  frame.setPixel(0, FrameBuffer::kHeight);
  TEST_ASSERT_EQUAL_INT(0, litCount(frame));
  TEST_ASSERT_FALSE(frame.pixel(-1, 0));
  TEST_ASSERT_FALSE(frame.pixel(0, FrameBuffer::kHeight));
}

void test_fill_rect_clips_at_edges() {
  FrameBuffer frame;
  frame.fillRect(120, 60, 20, 20);
  TEST_ASSERT_EQUAL_INT(8 * 4, litCount(frame));
}

void test_draw_rect_draws_outline_only() {
  FrameBuffer frame;
  frame.drawRect(10, 10, 5, 4);
  TEST_ASSERT_EQUAL_INT(14, litCount(frame));
  TEST_ASSERT_FALSE(frame.pixel(11, 11));
  frame.drawRect(0, 0, 0, 3);  // empty: draws nothing
  TEST_ASSERT_EQUAL_INT(14, litCount(frame));
}

void test_draw_pattern_is_transparent_and_scales() {
  const char *rows[] = {"#.", ".#"};
  FrameBuffer frame;
  frame.fillRect(0, 0, 2, 2);
  frame.drawPattern(0, 0, rows, 2, false);  // clears only the '#' cells
  TEST_ASSERT_FALSE(frame.pixel(0, 0));
  TEST_ASSERT_TRUE(frame.pixel(1, 0));

  FrameBuffer scaled;
  scaled.drawPattern(10, 10, rows, 2, true, 3);
  TEST_ASSERT_EQUAL_INT(18, litCount(scaled));
  TEST_ASSERT_TRUE(scaled.pixel(12, 12));
  TEST_ASSERT_TRUE(scaled.pixel(13, 13));
  TEST_ASSERT_FALSE(scaled.pixel(13, 10));
}

void test_draw_bitmap_reads_msb_first_rows() {
  const uint8_t bitmap[] = {0x80, 0x40,   // row 0: x=0, x=9
                            0x00, 0x01};  // row 1: x=15
  FrameBuffer frame;
  frame.drawBitmap(2, 3, bitmap, 16, 2);
  TEST_ASSERT_EQUAL_INT(3, litCount(frame));
  TEST_ASSERT_TRUE(frame.pixel(2, 3));
  TEST_ASSERT_TRUE(frame.pixel(11, 3));
  TEST_ASSERT_TRUE(frame.pixel(17, 4));
}

void test_equality_and_clear() {
  FrameBuffer a, b;
  TEST_ASSERT_TRUE(a == b);
  a.setPixel(1, 1);
  TEST_ASSERT_TRUE(a != b);
  a.clear();
  TEST_ASSERT_TRUE(a == b);
}

// ---- font ----------------------------------------------------------------

void test_text_width() {
  TEST_ASSERT_EQUAL_INT(0, StatusFont::textWidth(""));
  TEST_ASSERT_EQUAL_INT(11, StatusFont::textWidth("AB"));
  TEST_ASSERT_EQUAL_INT(22, StatusFont::textWidth("AB", 2));
}

void test_max_chars() {
  TEST_ASSERT_EQUAL_INT(21, StatusFont::maxChars(128));
  TEST_ASSERT_EQUAL_INT(10, StatusFont::maxChars(128, 2));
  TEST_ASSERT_EQUAL_INT(1, StatusFont::maxChars(5));
  TEST_ASSERT_EQUAL_INT(0, StatusFont::maxChars(4));
}

void test_draw_text_draws_glyph() {
  FrameBuffer frame;
  StatusFont::drawText(&frame, 0, 0, "L");
  // 'L': left column and bottom row.
  for (int y = 0; y < 7; ++y) TEST_ASSERT_TRUE(frame.pixel(0, y));
  for (int x = 0; x < 5; ++x) TEST_ASSERT_TRUE(frame.pixel(x, 6));
  TEST_ASSERT_EQUAL_INT(11, litCount(frame));
}

void test_draw_text_scales_and_advances() {
  FrameBuffer frame;
  StatusFont::drawText(&frame, 0, 0, "LL", 2);
  TEST_ASSERT_EQUAL_INT(2 * 11 * 4, litCount(frame));
  TEST_ASSERT_TRUE(frame.pixel(12, 0));  // second glyph starts at 12
}

void test_unknown_character_draws_question_mark() {
  FrameBuffer unknown, question;
  StatusFont::drawText(&unknown, 0, 0, "\x7f");
  StatusFont::drawText(&question, 0, 0, "?");
  TEST_ASSERT_TRUE(unknown == question);
}

void test_draw_text_off_clears_glyph_pixels() {
  FrameBuffer frame;
  frame.fillRect(0, 0, 10, 10);
  StatusFont::drawText(&frame, 0, 0, "L", 1, false);
  TEST_ASSERT_FALSE(frame.pixel(0, 0));
  TEST_ASSERT_TRUE(frame.pixel(1, 0));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_new_frame_is_blank);
  RUN_TEST(test_set_pixel_round_trips);
  RUN_TEST(test_bytes_use_ssd1306_page_layout);
  RUN_TEST(test_off_screen_pixels_are_ignored);
  RUN_TEST(test_fill_rect_clips_at_edges);
  RUN_TEST(test_draw_rect_draws_outline_only);
  RUN_TEST(test_draw_pattern_is_transparent_and_scales);
  RUN_TEST(test_draw_bitmap_reads_msb_first_rows);
  RUN_TEST(test_equality_and_clear);
  RUN_TEST(test_text_width);
  RUN_TEST(test_max_chars);
  RUN_TEST(test_draw_text_draws_glyph);
  RUN_TEST(test_draw_text_scales_and_advances);
  RUN_TEST(test_unknown_character_draws_question_mark);
  RUN_TEST(test_draw_text_off_clears_glyph_pixels);
  return UNITY_END();
}
