#include <unity.h>
#include <cstring>

#include "sequence_banner.h"

void setUp(void) {}
void tearDown(void) {}

DownlinkPacket downlink(const char *name, uint8_t id,
                        uint8_t progress = DownlinkPacket::kUnknown) {
  DownlinkPacket packet;
  std::strncpy(packet.sequenceName, name, sizeof(packet.sequenceName) - 1);
  packet.sequenceId = id;
  packet.sequenceProgressPercent = progress;
  return packet;
}

void test_hidden_before_any_sequence() {
  SequenceBanner banner;
  TEST_ASSERT_FALSE(banner.visible(0));
  banner.update(downlink("", 0), 100);
  TEST_ASSERT_FALSE(banner.visible(100));
}

void test_without_progress_shows_for_five_seconds() {
  SequenceBanner banner;
  banner.update(downlink("", 0), 0);
  banner.update(downlink("Happy", 1), 1000);
  TEST_ASSERT_TRUE(banner.visible(1000));
  TEST_ASSERT_EQUAL_STRING("Happy", banner.name());
  TEST_ASSERT_EQUAL_INT(-1, banner.progressPercent());
  banner.update(downlink("Happy", 1), 5999);
  TEST_ASSERT_TRUE(banner.visible(5999));
  TEST_ASSERT_FALSE(banner.visible(6000));
}

// The window is fixed: Amidala builds without progress may stop sending
// the name right away, or keep sending it long after.
void test_window_ignores_name_clearing_and_repeats() {
  SequenceBanner banner;
  banner.update(downlink("Happy", 1), 0);
  banner.update(downlink("", 1), 50);
  TEST_ASSERT_TRUE(banner.visible(4000));
  TEST_ASSERT_EQUAL_STRING("Happy", banner.name());
  banner.update(downlink("Happy", 1), 6000);
  TEST_ASSERT_FALSE(banner.visible(6000));
}

void test_new_id_restarts_window_even_for_same_name() {
  SequenceBanner banner;
  banner.update(downlink("Happy", 1), 0);
  banner.update(downlink("Happy", 2), 8000);
  TEST_ASSERT_TRUE(banner.visible(12000));
  TEST_ASSERT_FALSE(banner.visible(13000));
}

void test_new_sequence_replaces_name() {
  SequenceBanner banner;
  banner.update(downlink("Happy", 1), 0);
  banner.update(downlink("Scream", 2), 1000);
  TEST_ASSERT_EQUAL_STRING("Scream", banner.name());
}

void test_with_progress_stays_until_name_clears() {
  SequenceBanner banner;
  banner.update(downlink("Leia msg", 3, 10), 0);
  banner.update(downlink("Leia msg", 3, 90), 20000);
  TEST_ASSERT_TRUE(banner.visible(20000));
  TEST_ASSERT_EQUAL_INT(90, banner.progressPercent());
  banner.update(downlink("", 3, 100), 21000);
  TEST_ASSERT_FALSE(banner.visible(21000));
}

void test_progress_over_100_is_clamped() {
  SequenceBanner banner;
  banner.update(downlink("Leia msg", 3, 250), 0);
  TEST_ASSERT_EQUAL_INT(100, banner.progressPercent());
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_hidden_before_any_sequence);
  RUN_TEST(test_without_progress_shows_for_five_seconds);
  RUN_TEST(test_window_ignores_name_clearing_and_repeats);
  RUN_TEST(test_new_id_restarts_window_even_for_same_name);
  RUN_TEST(test_new_sequence_replaces_name);
  RUN_TEST(test_with_progress_stays_until_name_clears);
  RUN_TEST(test_progress_over_100_is_clamped);
  return UNITY_END();
}
