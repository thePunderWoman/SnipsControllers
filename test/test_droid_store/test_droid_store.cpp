#include <unity.h>
#include <cstring>

#include "droid_store.h"

void setUp(void) {}
void tearDown(void) {}

void test_starts_empty() {
  DroidStore store;
  TEST_ASSERT_EQUAL_INT(0, store.count());
}

void test_add_appends_entry() {
  DroidStore store;
  TEST_ASSERT_TRUE(store.add("R2-D2", "0013A20041A7B3C2"));
  TEST_ASSERT_EQUAL_INT(1, store.count());
  TEST_ASSERT_EQUAL_STRING("R2-D2", store.at(0).name);
  TEST_ASSERT_EQUAL_STRING("0013A20041A7B3C2", store.at(0).panId);
}

void test_add_multiple_preserves_order() {
  DroidStore store;
  store.add("R2-D2", "1111111111111111");
  store.add("BB-8", "2222222222222222");
  TEST_ASSERT_EQUAL_STRING("R2-D2", store.at(0).name);
  TEST_ASSERT_EQUAL_STRING("BB-8", store.at(1).name);
}

void test_add_truncates_overly_long_fields() {
  DroidStore store;
  store.add("ThisNameIsWayTooLongForTheField", "0");
  TEST_ASSERT_EQUAL_INT(DroidEntry::kMaxNameLength,
                        std::strlen(store.at(0).name));
}

void test_add_fails_past_capacity() {
  DroidStore store;
  for (size_t i = 0; i < DroidStore::kMaxDroids; ++i) {
    TEST_ASSERT_TRUE(store.add("X", "0"));
  }
  TEST_ASSERT_FALSE(store.add("Overflow", "0"));
  TEST_ASSERT_EQUAL_INT(DroidStore::kMaxDroids, store.count());
}

void test_at_out_of_range_returns_empty_entry() {
  DroidStore store;
  store.add("R2-D2", "1111111111111111");
  TEST_ASSERT_EQUAL_STRING("", store.at(5).name);
}

void test_remove_shifts_subsequent_entries_down() {
  DroidStore store;
  store.add("R2-D2", "1111111111111111");
  store.add("BB-8", "2222222222222222");
  store.add("C-3PO", "3333333333333333");

  TEST_ASSERT_TRUE(store.remove(0));
  TEST_ASSERT_EQUAL_INT(2, store.count());
  TEST_ASSERT_EQUAL_STRING("BB-8", store.at(0).name);
  TEST_ASSERT_EQUAL_STRING("C-3PO", store.at(1).name);
}

void test_remove_out_of_range_is_noop() {
  DroidStore store;
  store.add("R2-D2", "1111111111111111");
  TEST_ASSERT_FALSE(store.remove(5));
  TEST_ASSERT_EQUAL_INT(1, store.count());
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_starts_empty);
  RUN_TEST(test_add_appends_entry);
  RUN_TEST(test_add_multiple_preserves_order);
  RUN_TEST(test_add_truncates_overly_long_fields);
  RUN_TEST(test_add_fails_past_capacity);
  RUN_TEST(test_at_out_of_range_returns_empty_entry);
  RUN_TEST(test_remove_shifts_subsequent_entries_down);
  RUN_TEST(test_remove_out_of_range_is_noop);
  return UNITY_END();
}
