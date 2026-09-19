#include <unity.h>

#include "pan_id.h"

void setUp(void) {}
void tearDown(void) {}

void test_normalize_left_pads_short_id_with_zeros() {
  char out[PanId::kHexLength + 1];
  TEST_ASSERT_TRUE(PanId::normalize("4133", out));
  TEST_ASSERT_EQUAL_STRING("0000000000004133", out);
}

void test_normalize_leaves_full_length_id_unchanged() {
  char out[PanId::kHexLength + 1];
  TEST_ASSERT_TRUE(PanId::normalize("0013A20041A7B3C2", out));
  TEST_ASSERT_EQUAL_STRING("0013A20041A7B3C2", out);
}

void test_normalize_accepts_single_digit() {
  char out[PanId::kHexLength + 1];
  TEST_ASSERT_TRUE(PanId::normalize("1", out));
  TEST_ASSERT_EQUAL_STRING("0000000000000001", out);
}

void test_normalize_uppercases_lowercase_hex() {
  char out[PanId::kHexLength + 1];
  TEST_ASSERT_TRUE(PanId::normalize("ab12", out));
  TEST_ASSERT_EQUAL_STRING("000000000000AB12", out);
}

void test_normalize_accepts_all_zeros() {
  // Factory Reset's "unconfigured" PAN ID must still be sendable.
  char out[PanId::kHexLength + 1];
  TEST_ASSERT_TRUE(PanId::normalize("0000000000000000", out));
  TEST_ASSERT_EQUAL_STRING("0000000000000000", out);
}

void test_normalize_rejects_null_empty_overlong_and_non_hex() {
  char out[PanId::kHexLength + 1] = "untouched";
  TEST_ASSERT_FALSE(PanId::normalize(nullptr, out));
  TEST_ASSERT_FALSE(PanId::normalize("", out));
  TEST_ASSERT_FALSE(PanId::normalize("00000000000000001", out));
  TEST_ASSERT_FALSE(PanId::normalize("41G3", out));
  TEST_ASSERT_FALSE(PanId::normalize("41 3", out));
  TEST_ASSERT_EQUAL_STRING("untouched", out);
}

void test_equivalent_matches_short_and_padded_forms() {
  // Regression: the XBee reports "0000000000004133" but the saved droid
  // holds "4133"; a plain strcmp never matched them at boot.
  TEST_ASSERT_TRUE(PanId::equivalent("4133", "0000000000004133"));
  TEST_ASSERT_TRUE(PanId::equivalent("0000000000004133", "4133"));
  TEST_ASSERT_TRUE(PanId::equivalent("4133", "4133"));
  TEST_ASSERT_TRUE(PanId::equivalent("abcd", "ABCD"));
}

void test_equivalent_rejects_different_or_invalid() {
  TEST_ASSERT_FALSE(PanId::equivalent("4133", "4134"));
  TEST_ASSERT_FALSE(PanId::equivalent("4133", "1400000000004133"));
  TEST_ASSERT_FALSE(PanId::equivalent("", ""));
  TEST_ASSERT_FALSE(PanId::equivalent("zz", "zz"));
  TEST_ASSERT_FALSE(PanId::equivalent(nullptr, "4133"));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_normalize_left_pads_short_id_with_zeros);
  RUN_TEST(test_normalize_leaves_full_length_id_unchanged);
  RUN_TEST(test_normalize_accepts_single_digit);
  RUN_TEST(test_normalize_uppercases_lowercase_hex);
  RUN_TEST(test_normalize_accepts_all_zeros);
  RUN_TEST(test_normalize_rejects_null_empty_overlong_and_non_hex);
  RUN_TEST(test_equivalent_matches_short_and_padded_forms);
  RUN_TEST(test_equivalent_rejects_different_or_invalid);
  return UNITY_END();
}
