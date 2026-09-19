#include <unity.h>

#include <string>

#include "droid_switcher.h"

void setUp(void) {}
void tearDown(void) {}

namespace {

// Configurable fake so tests can force a failure at any step and verify
// the orchestration stops there instead of calling subsequent steps.
class FakeTransport : public XbeeTransport {
 public:
  bool leaveResult = true;
  bool setPanResult = true;
  bool rejoinResult = true;

  bool leaveCalled = false;
  bool setPanCalled = false;
  bool rejoinCalled = false;
  std::string lastPanId;

  bool leaveNetwork() override {
    leaveCalled = true;
    return leaveResult;
  }
  bool setPanId(const char *panId) override {
    setPanCalled = true;
    lastPanId = panId;
    return setPanResult;
  }
  bool rejoinNetwork() override {
    rejoinCalled = true;
    return rejoinResult;
  }
};

}  // namespace

void test_switch_succeeds_when_every_step_succeeds() {
  FakeTransport transport;
  TEST_ASSERT_TRUE(DroidSwitchResult::kSuccess ==
                    DroidSwitcher::switchTo("1111111111111111", &transport));
  TEST_ASSERT_TRUE(transport.leaveCalled);
  TEST_ASSERT_TRUE(transport.setPanCalled);
  TEST_ASSERT_TRUE(transport.rejoinCalled);
  TEST_ASSERT_EQUAL_STRING("1111111111111111", transport.lastPanId.c_str());
}

void test_switch_with_null_transport_fails_without_crashing() {
  TEST_ASSERT_TRUE(DroidSwitchResult::kNoTransport ==
                    DroidSwitcher::switchTo("1111111111111111", nullptr));
}

void test_switch_stops_after_leave_failure() {
  FakeTransport transport;
  transport.leaveResult = false;
  TEST_ASSERT_TRUE(DroidSwitchResult::kLeaveFailed ==
                    DroidSwitcher::switchTo("1111111111111111", &transport));
  TEST_ASSERT_TRUE(transport.leaveCalled);
  TEST_ASSERT_FALSE(transport.setPanCalled);
  TEST_ASSERT_FALSE(transport.rejoinCalled);
}

void test_switch_stops_after_set_pan_failure() {
  FakeTransport transport;
  transport.setPanResult = false;
  TEST_ASSERT_TRUE(DroidSwitchResult::kSetPanFailed ==
                    DroidSwitcher::switchTo("1111111111111111", &transport));
  TEST_ASSERT_TRUE(transport.setPanCalled);
  TEST_ASSERT_FALSE(transport.rejoinCalled);
}

void test_switch_reports_rejoin_failure() {
  FakeTransport transport;
  transport.rejoinResult = false;
  TEST_ASSERT_TRUE(DroidSwitchResult::kRejoinFailed ==
                    DroidSwitcher::switchTo("1111111111111111", &transport));
}

void test_switch_pads_short_pan_id_to_full_length() {
  // Regression: "4133" reached XbeeControl::setPanId as-is, which needs
  // exactly 16 hex digits, so switching failed with "Set PAN failed".
  FakeTransport transport;
  TEST_ASSERT_TRUE(DroidSwitchResult::kSuccess ==
                    DroidSwitcher::switchTo("4133", &transport));
  TEST_ASSERT_EQUAL_STRING("0000000000004133", transport.lastPanId.c_str());
}

void test_switch_rejects_invalid_pan_id_before_touching_the_network() {
  FakeTransport transport;
  TEST_ASSERT_TRUE(DroidSwitchResult::kInvalidPanId ==
                    DroidSwitcher::switchTo("", &transport));
  TEST_ASSERT_TRUE(DroidSwitchResult::kInvalidPanId ==
                    DroidSwitcher::switchTo("41G3", &transport));
  TEST_ASSERT_FALSE(transport.leaveCalled);
  TEST_ASSERT_FALSE(transport.setPanCalled);
  TEST_ASSERT_FALSE(transport.rejoinCalled);
}

// XbeeControl itself (the real XbeeTransport, using XbeeSpi) is
// hardware-dependent now and excluded from native builds — see
// platformio.ini. Nothing here instantiates it directly; DroidSwitcher is
// tested purely against FakeTransport above.

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_switch_succeeds_when_every_step_succeeds);
  RUN_TEST(test_switch_with_null_transport_fails_without_crashing);
  RUN_TEST(test_switch_stops_after_leave_failure);
  RUN_TEST(test_switch_stops_after_set_pan_failure);
  RUN_TEST(test_switch_reports_rejoin_failure);
  RUN_TEST(test_switch_pads_short_pan_id_to_full_length);
  RUN_TEST(test_switch_rejects_invalid_pan_id_before_touching_the_network);
  return UNITY_END();
}
